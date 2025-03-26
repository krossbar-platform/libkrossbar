#pragma once

#include "message.h"
#include "../event_manager.h"

struct kb_transport_uds_s;
struct io_uring;
struct io_uring_cqe;

struct kb_event_manager_uds_s
{
    kb_event_manager_t base;

    kb_event_t read_event;
    kb_event_t write_event;
};

typedef struct kb_event_manager_uds_s kb_event_manager_uds_t;

kb_event_manager_uds_t *event_manager_uds_create(struct kb_transport_uds_s *transport, struct io_uring *ring, log4c_category_t *logger);
void event_manager_uds_destroy(kb_event_manager_uds_t *manager);
void event_manager_uds_wait_readable(kb_event_manager_uds_t *manager);
void event_manager_uds_wait_writeable(kb_event_manager_uds_t *manager);
kb_message_t *event_manager_uds_handle_event(struct io_uring_cqe *cqe);
