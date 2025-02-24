#pragma once

#include "message.h"
#include "../event_manager.h"

struct kb_transport_uds_s;
struct io_uring;
struct io_uring_cqe;

enum kb_uds_event_type_u
{
    KB_UDS_EVENT_READABLE,
    KB_UDS_EVENT_WRITEABLE,
    KB_UDS_EVENT_MAX
};

typedef enum kb_uds_event_type_u kb_uds_event_type_t;

struct kb_event_manager_uds_s;
struct kb_uds_event_s
{
    struct kb_event_manager_uds_s *manager;
    kb_uds_event_type_t event_type;
};

typedef struct kb_uds_event_s kb_uds_event_t;

struct kb_event_manager_uds_s
{
    kb_event_manager_t base;
    struct kb_transport_uds_s *transport;
    struct io_uring *ring;

    kb_uds_event_t read_event;
    kb_uds_event_t write_event;
};

typedef struct kb_event_manager_uds_s kb_event_manager_uds_t;

void event_manager_uds_init(kb_event_manager_uds_t *manager, struct kb_transport_uds_s *transport, struct io_uring *ring);
void event_manager_uds_wait_readable(kb_event_manager_uds_t *manager);
void event_manager_uds_wait_writeable(kb_event_manager_uds_t *manager);
kb_message_t *event_manager_uds_handle_event(struct io_uring_cqe *cqe);
