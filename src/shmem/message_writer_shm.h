#pragma once

#include "message_writer.h"
#include "transport_shm.h"

struct kb_message_writer_shm_s
{
    kb_message_writer_t base;
    kb_message_header_t *header;
    kb_transport_shm_t *transport;
};

typedef struct kb_message_writer_shm_s kb_message_writer_shm_t;

kb_message_writer_shm_t *message_writer_shm_init(kb_transport_shm_t *transport,
                                                 kb_message_header_t *header,
                                                 char *buffer);
int message_writer_shm_send(kb_message_writer_t *writer);
void message_writer_shm_cancel(kb_message_writer_t *writer);