#pragma once

#include "transport.h"

struct kb_peer_s {
    kb_transport_t *transport;
};

typedef struct kb_peer_s kb_peer_t;

kb_peer_t* krossbar_peer_init();
void krossbar_peer_destroy(kb_peer_t *peer);
