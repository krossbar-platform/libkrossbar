#include "event_manager_shm.h"

#include <assert.h>
#include <linux/futex.h>

#include <liburing.h>
#include <log4c.h>

#include "transport_shm.h"
#include "../utils.h"

kb_event_manager_shm_t *event_manager_shm_create(struct kb_transport_shm_s *transport, struct io_uring *ring, log4c_category_t *logger)
{
    assert(transport != NULL);
    assert(ring != NULL);

    kb_event_manager_shm_t *manager = malloc(sizeof(kb_event_manager_shm_t));
    if (manager == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "malloc failed");
        return NULL;
    }

    struct io_uring_probe *probe = io_uring_get_probe_ring(ring);
    if (!probe)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to get probe: %s", strerror(errno));
        return NULL;
    }

    if (!io_uring_opcode_supported(probe, IORING_OP_FUTEX_WAIT))
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "IORING_OP_FUTEX_WAIT not supported");
        return NULL;
    }

    if (!io_uring_opcode_supported(probe, IORING_OP_FUTEX_WAKE))
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "IORING_OP_FUTEX_WAKE not supported");
        return NULL;
    }

    free(probe);

    manager->read_event.manager = (kb_event_manager_t *)manager;
    manager->read_event.event_type = KB_UDS_EVENT_READABLE;

    manager->write_event.manager = (kb_event_manager_t *)manager;
    manager->write_event.event_type = KB_UDS_EVENT_WRITEABLE;

    manager->base.transport = (kb_transport_t *)transport;
    manager->base.ring = ring;
    manager->base.logger = logger;
    manager->base.handle_event = event_manager_shm_handle_event;

    return manager;
}

void event_manager_shm_destroy(kb_event_manager_shm_t *manager)
{
    if (manager)
    {
        free(manager);
    }
}

void event_manager_shm_signal_new_message(kb_event_manager_shm_t *manager)
{
    struct io_uring *ring = manager->base.ring;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_sqe_set_data(sqe, &manager->read_event);

    kb_transport_shm_t *transport = (kb_transport_shm_t *)manager->base.transport;
    kb_arena_header_t *header = transport->write_arena.header;

    log_trace(manager->base.logger, "Signalling %p", &header->num_messages);
    io_uring_prep_futex_wake(sqe, &header->num_messages, 1, FUTEX_BITSET_MATCH_ANY, FUTEX2_SIZE_U32, 0);

    int ret = io_uring_submit(ring);
    if (ret < 0)
    {
        log4c_category_log(manager->base.logger, LOG4C_PRIORITY_ERROR, "io_uring futex wake submit error: %s", strerror(-ret));
    }

    // Wait for completion
    struct io_uring_cqe *cqe;
    ret = io_uring_wait_cqe(ring, &cqe);

    if (ret < 0)
    {
        log4c_category_log(manager->base.logger, LOG4C_PRIORITY_ERROR, "io_uring futex wake error: %s", strerror(-ret));
    }
    else if (cqe->res < 0)
    {
        log4c_category_log(manager->base.logger, LOG4C_PRIORITY_ERROR, "io_uring_prep_futex_wake error: %s", strerror(-cqe->res));
    }

    io_uring_cqe_seen(ring, cqe);
}

void event_manager_shm_wait_messages(kb_event_manager_shm_t *manager)
{
    struct io_uring *ring = manager->base.ring;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_sqe_set_data(sqe, &manager->write_event);

    kb_transport_shm_t *transport = (kb_transport_shm_t *)manager->base.transport;
    kb_arena_header_t *header = transport->read_arena.header;

    log_trace(manager->base.logger, "Waiting %p", &header->num_messages);
    io_uring_prep_futex_wait(sqe, &header->num_messages, 0, FUTEX_BITSET_MATCH_ANY, FUTEX2_SIZE_U32, 0);

    int ret = io_uring_submit(ring);
    if (ret < 0)
    {
        log4c_category_log(manager->base.logger, LOG4C_PRIORITY_ERROR, "io_uring futex wait submit error: %s", strerror(-ret));
    }
}

kb_message_t *event_manager_shm_handle_event(struct io_uring_cqe *cqe)
{
    kb_event_t *event = (kb_event_t *)io_uring_cqe_get_data(cqe);
    kb_event_manager_shm_t *self = (kb_event_manager_shm_t *)event->manager;

    if (cqe->res == EINTR || cqe->res == EAGAIN)
    {
        event_manager_shm_wait_messages(self);
        return NULL;
    }

    return transport_shm_message_receive(event->manager->transport);
}