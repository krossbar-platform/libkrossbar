#pragma once

#include "message.h"
#include "../event_manager.h"

struct kb_transport_shm_s;
struct io_uring;
struct io_uring_cqe;

struct kb_event_manager_shm_s
{
    kb_event_manager_t base;
    struct kb_transport_shm_s *transport;
    struct io_uring *ring;
};

typedef struct kb_event_manager_shm_s kb_event_manager_shm_t;

kb_event_manager_shm_t *event_manager_shm_create(struct kb_transport_shm_s *transport, struct io_uring *ring, log4c_category_t *logger);
void event_manager_shm_destroy(kb_event_manager_shm_t *);
void event_manager_shm_wait_messages(kb_event_manager_shm_t *manager);
void event_manager_shm_signal_new_message(kb_event_manager_shm_t *manager);
kb_message_t *event_manager_shm_handle_event(struct io_uring_cqe *cqe);