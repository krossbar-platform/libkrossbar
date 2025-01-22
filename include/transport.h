#pragma once

#include "message_writer.h"
#include "message.h"

struct kb_transport_s {
    kb_message_writer_t* (*message_init)(struct kb_transport_s *transport);
    int (*message_send)(struct kb_transport_s *transport, kb_message_writer_t *writer);
    kb_message_t *(*message_receive)(struct kb_transport_s *transport);
    int (*message_release)(struct kb_transport_s *transport, kb_message_t *writer);
    int (*get_fd)(struct kb_transport_s *transport);
    void (*destroy)(struct kb_transport_s *transport);
};

typedef struct kb_transport_s kb_transport_t;
