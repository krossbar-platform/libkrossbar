#pragma once

#include <log4c/category.h>

#include "event_manager.h"
#include "message_writer.h"
#include "message.h"

struct kb_transport_s
{
    const char *name;
    log4c_category_t *logger;
    kb_event_manager_t *event_manager;
    kb_message_writer_t *(*message_init)(struct kb_transport_s *transport);
    kb_message_t *(*message_receive)(struct kb_transport_s *transport);
    void (*destroy)(struct kb_transport_s *transport);
};

typedef struct kb_transport_s kb_transport_t;

inline kb_message_writer_t *transport_message_init(kb_transport_t *transport)
{
    return transport->message_init(transport);
}

inline kb_event_manager_t *transport_get_event_manager(kb_transport_t *transport)
{
    return transport->event_manager;
}

inline kb_message_t *transport_message_receive(kb_transport_t *transport)
{
    return transport->message_receive(transport);
}

inline void transport_destroy(kb_transport_t *transport)
{
    transport->destroy(transport);
}