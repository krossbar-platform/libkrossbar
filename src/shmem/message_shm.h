#pragma once

#include "message.h"
#include "transport_shm.h"

struct kb_message_shm_s
{
    kb_message_t base;
    kb_message_header_t *header;
    kb_transport_shm_t *transport;
};

typedef struct kb_message_shm_s kb_message_shm_t;

kb_message_shm_t *message_shm_init(kb_transport_shm_t *transport,
                                   kb_message_header_t *header,
                                   char *buffer);
int message_shm_clean(kb_message_t *message);
