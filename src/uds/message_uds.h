#pragma once

#include "message.h"
#include "transport_uds.h"

struct kb_message_uds_s
{
    kb_message_t base;
    kb_transport_uds_t *transport;
};

typedef struct kb_message_uds_s kb_message_uds_t;

kb_message_uds_t *message_uds_init(kb_transport_uds_t *transport,
                                   char *buffer,
                                   size_t size);
int message_uds_clean(kb_message_t *message);