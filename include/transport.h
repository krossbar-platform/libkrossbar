#pragma once

#include "message_writer.h"

struct kb_transport_s {
    kb_message_writer_t* (*message_init)(struct kb_transport_s *transport);
    int (*message_send)(struct kb_transport_s *transport, kb_message_writer_t *writer);
    int (*get_fd)(struct kb_transport_s *transport);
    void (*destroy)(struct kb_transport_s *transport);
};

typedef struct kb_transport_s kb_transport_t;
