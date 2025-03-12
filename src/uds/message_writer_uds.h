#pragma once

#include "message_writer.h"
#include "transport_uds.h"

struct kb_message_writer_uds_s
{
    kb_message_writer_t base;
    kb_transport_uds_t *transport;
};

typedef struct kb_message_writer_uds_s kb_message_writer_uds_t;

kb_message_writer_uds_t *message_writer_uds_init(kb_transport_uds_t *transport,
                                                 char *buffer,
                                                 size_t buffer_size);
int message_writer_uds_send(kb_message_writer_t *writer);
void message_writer_uds_cancel(kb_message_writer_t *writer);