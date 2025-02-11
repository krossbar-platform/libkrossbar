#include "event_manager_shm.h"

#include <assert.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#include <liburing.h>

#include "transport_shm.h"

void event_manager_shm_init(kb_event_manager_shm_t *manager, struct kb_transport_shm_s *transport, struct io_uring *ring)
{
    assert(manager != NULL);
    assert(transport != NULL);
    assert(ring != NULL);

    if (io_uring_check_version(2, 6))
    {
        fprintf(stderr, "liburing version is too old\n");
        exit(EXIT_FAILURE);
    }


    manager->transport = transport;
    manager->ring = ring;
    manager->base.handle_event = event_manager_shm_handle_event;
}

void event_manager_shm_signal_new_message(kb_event_manager_shm_t *manager)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(manager->ring);

    kb_arena_header_t *header = manager->transport->arena.header;
    io_uring_prep_futex_wake(sqe, &header->num_messages, 1, FUTEX_BITSET_MATCH_ANY, 0, 0);

    if (io_uring_submit(manager->ring) == -errno)
    {
        perror("io_uring_submit failed");
    }
}

void event_manager_shm_wait_messages(kb_event_manager_shm_t *manager)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(manager->ring);
    io_uring_sqe_set_data(sqe, manager);

    kb_arena_header_t *header = manager->transport->arena.header;
    io_uring_prep_futex_wait(sqe, &header->num_messages, 0, FUTEX_BITSET_MATCH_ANY, 0, 0);

    if (io_uring_submit(manager->ring) == -errno)
    {
        perror("io_uring_submit failed");
    }
}

kb_message_t *event_manager_shm_handle_event(kb_event_manager_t *manager, struct io_uring_cqe *cqe)
{
    kb_event_manager_shm_t *self = (kb_event_manager_shm_t *)manager;

    return transport_shm_message_receive(&self->transport->base);
}