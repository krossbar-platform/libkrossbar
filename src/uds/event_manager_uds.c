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
}

void event_manager_uds_wait_messages(kb_event_manager_uds_t *manager)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(manager->ring);
    io_uring_sqe_set_data(sqe, manager);

    io_uring_prep_recv(sqe, transport_uds_get_fd(&manager->transport->base), NULL, 0, 0);

    int ret = io_uring_submit(manager->ring);
    if (ret < 0)
    {
        fprintf(stderr, "io_uring futex wait submit error: %s\n", strerror(-ret));
    }
}

kb_message_t *event_manager_uds_handle_event(kb_event_manager_t *manager, struct io_uring_cqe *cqe)
{
    kb_event_manager_uds_t *self = (kb_event_manager_uds_t *)manager;

    return transport_uds_message_receive(&self->transport->base);
}