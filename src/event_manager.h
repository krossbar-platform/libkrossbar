#pragma once

struct io_uring_cqe;
struct kb_message_s;

struct kb_event_manager_s
{
    struct kb_message_s *(*handle_event)(struct kb_event_manager_s *manager, struct io_uring_cqe *cqe);
};

typedef struct kb_event_manager_s kb_event_manager_t;