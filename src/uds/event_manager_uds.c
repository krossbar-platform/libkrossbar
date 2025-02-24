#include "event_manager_uds.h"

#include <assert.h>
#include <linux/futex.h>

#include <liburing.h>

#include "transport_uds.h"

void event_manager_uds_init(kb_event_manager_uds_t *manager, struct kb_transport_uds_s *transport, struct io_uring *ring)
{
    assert(manager != NULL);
    assert(transport != NULL);
    assert(ring != NULL);

    manager->transport = transport;
    manager->ring = ring;
    manager->base.handle_event = event_manager_uds_handle_event;

    event_manager_uds_wait_readable(manager);
}

void event_manager_uds_wait_readable(kb_event_manager_uds_t *manager)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(manager->ring);

    kb_uds_event_t *event = malloc(sizeof(kb_uds_event_t));
    event->manager = manager;
    event->event_type = KB_UDS_EVENT_READABLE;

    io_uring_sqe_set_data(sqe, event);

    io_uring_prep_recv(sqe, transport_uds_get_fd(&manager->transport->base), NULL, 0, 0);

    int ret = io_uring_submit(manager->ring);
    if (ret < 0)
    {
        fprintf(stderr, "io_uring futex wait submit error: %s\n", strerror(-ret));
    }
}

void event_manager_uds_wait_writeable(kb_event_manager_uds_t *manager)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(manager->ring);

    kb_uds_event_t *event = malloc(sizeof(kb_uds_event_t));
    event->manager = manager;
    event->event_type = KB_UDS_EVENT_WRITEABLE;

    io_uring_sqe_set_data(sqe, event);

    io_uring_prep_send(sqe, transport_uds_get_fd(&manager->transport->base), NULL, 0, 0);

    int ret = io_uring_submit(manager->ring);
    if (ret < 0)
    {
        fprintf(stderr, "io_uring futex wait submit error: %s\n", strerror(-ret));
    }
}

kb_message_t *event_manager_uds_handle_event(struct io_uring_cqe *cqe)
{
    kb_uds_event_t *event = (kb_uds_event_t *)io_uring_cqe_get_data(cqe);

    kb_event_manager_uds_t *self = event->manager;
    int event_type = event->event_type;
    free(event);

    if (event_type == KB_UDS_EVENT_READABLE)
    {
        event_manager_uds_wait_readable(self);

        if (cqe->res == EINTR || cqe->res == EAGAIN)
        {
            return NULL;
        }
        return transport_uds_message_receive(&self->transport->base);
    }
    else if (event_type == KB_UDS_EVENT_WRITEABLE)
    {
        if (cqe->res == EINTR || cqe->res == EAGAIN)
        {
            event_manager_uds_wait_writeable(self);
            return NULL;
        }

        if (transport_uds_write_messages(&self->transport->base) > 0)
        {
            event_manager_uds_wait_writeable(self);
        }

        return NULL;
    }
}