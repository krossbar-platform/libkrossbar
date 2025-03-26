#pragma once

#include <log4c/category.h>

#include "message.h"

struct io_uring_cqe;
struct io_uring;

struct kb_event_manager_s
{
    log4c_category_t *logger;
    struct kb_transport_s *transport;
    struct io_uring *ring;
    kb_message_t *(*handle_event)(struct io_uring_cqe *cqe);
};

typedef struct kb_event_manager_s kb_event_manager_t;

enum kb_event_type_u
{
    KB_UDS_EVENT_READABLE,
    KB_UDS_EVENT_WRITEABLE,
    KB_UDS_EVENT_MAX
};

typedef enum kb_event_type_u kb_event_type_t;

struct kb_event_s
{
    kb_event_manager_t *manager;
    kb_event_type_t event_type;
};

typedef struct kb_event_s kb_event_t;

inline kb_message_t *event_manager_handle_event(kb_event_manager_t *manager, struct io_uring_cqe *cqe)
{
    return manager->handle_event(cqe);
}