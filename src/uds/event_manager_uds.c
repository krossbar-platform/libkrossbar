#include "event_manager_uds.h"

#include <assert.h>
#include <linux/futex.h>

#include <liburing.h>
#include <log4c.h>

#include "transport_uds.h"

kb_event_manager_uds_t *event_manager_uds_create(struct kb_transport_uds_s *transport, struct io_uring *ring, log4c_category_t *logger)
{
    assert(transport != NULL);
    assert(ring != NULL);

    kb_event_manager_uds_t *manager = malloc(sizeof(kb_event_manager_uds_t));
    if (manager == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "malloc failed");
        return NULL;
    }

    manager->base.transport = (kb_transport_t *)transport;
    manager->base.ring = ring;
    manager->base.logger = logger;
    manager->base.handle_event = event_manager_uds_handle_event;

    manager->read_event.manager = (kb_event_manager_t *)manager;
    manager->read_event.event_type = KB_UDS_EVENT_READABLE;

    manager->write_event.manager = (kb_event_manager_t *)manager;
    manager->write_event.event_type = KB_UDS_EVENT_WRITEABLE;

    event_manager_uds_wait_readable(manager);

    return manager;
}

void event_manager_uds_destroy(kb_event_manager_uds_t *manager)
{
    assert(manager != NULL);

    if (manager)
    {
        free(manager);
    }
}

void event_manager_uds_wait_readable(kb_event_manager_uds_t *manager)
{
    assert(manager != NULL);

    struct io_uring *ring = manager->base.ring;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    io_uring_sqe_set_data(sqe, &manager->read_event);

    kb_transport_uds_t *transport = (kb_transport_uds_t *)manager->base.transport;

    io_uring_prep_recv(sqe, transport->sock_fd, NULL, 0, 0);

    int ret = io_uring_submit(ring);
    if (ret < 0)
    {
        log4c_category_log(manager->base.logger, LOG4C_PRIORITY_ERROR, "io_uring futex wait submit error: %s", strerror(-ret));
    }
}

void event_manager_uds_wait_writeable(kb_event_manager_uds_t *manager)
{
    assert(manager != NULL);

    struct io_uring *ring = manager->base.ring;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    io_uring_sqe_set_data(sqe, &manager->write_event);

    kb_transport_uds_t *transport = (kb_transport_uds_t *)manager->base.transport;

    io_uring_prep_send(sqe, transport->sock_fd, NULL, 0, 0);

    int ret = io_uring_submit(ring);
    if (ret < 0)
    {
        log4c_category_log(manager->base.logger, LOG4C_PRIORITY_ERROR, "io_uring futex wait submit error: %s", strerror(-ret));
    }
}

kb_message_t *event_manager_uds_handle_event(struct io_uring_cqe *cqe)
{
    assert(cqe != NULL);

    kb_event_t *event = (kb_event_t *)io_uring_cqe_get_data(cqe);

    kb_event_manager_uds_t *self = (kb_event_manager_uds_t *)event->manager;

    if (event->event_type == KB_UDS_EVENT_READABLE)
    {
        event_manager_uds_wait_readable(self);

        if (cqe->res == EINTR || cqe->res == EAGAIN)
        {
            return NULL;
        }
        return transport_uds_message_receive(event->manager->transport);
    }
    else if (event->event_type == KB_UDS_EVENT_WRITEABLE)
    {
        if (cqe->res == EINTR || cqe->res == EAGAIN)
        {
            event_manager_uds_wait_writeable(self);
            return NULL;
        }

        if (transport_uds_write_messages(event->manager->transport) > 0)
        {
            event_manager_uds_wait_writeable(self);
        }

        return NULL;
    }
}