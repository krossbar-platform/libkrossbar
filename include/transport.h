#pragma once

#include <log4c/category.h>

#include "message_writer.h"
#include "message.h"

struct kb_transport_s {
    log4c_category_t *logger;
    kb_message_writer_t *(*message_init)(struct kb_transport_s *transport);
    kb_message_t *(*message_receive)(struct kb_transport_s *transport);
    int (*get_fd)(struct kb_transport_s *transport);
    void (*destroy)(struct kb_transport_s *transport);
};

typedef struct kb_transport_s kb_transport_t;

inline kb_message_writer_t *transport_message_init(kb_transport_t *transport)
{
    return transport->message_init(transport);
}

inline kb_message_t *transport_message_receive(kb_transport_t *transport)
{
    return transport->message_receive(transport);
}

inline int transport_get_fd(kb_transport_t *transport)
{
    return transport->get_fd(transport);
}

inline void transport_destroy(kb_transport_t *transport)
{
    transport->destroy(transport);
}