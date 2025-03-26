#pragma once

#include <log4c/category.h>

#include "message.h"

struct io_uring_cqe;

struct kb_event_manager_s
{
    log4c_category_t *logger;
    kb_message_t *(*handle_event)(struct io_uring_cqe *cqe);
};

typedef struct kb_event_manager_s kb_event_manager_t;

inline kb_message_t *event_manager_handle_event(kb_event_manager_t *manager, struct io_uring_cqe *cqe)
{
    return manager->handle_event(cqe);
}