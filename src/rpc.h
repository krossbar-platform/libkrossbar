#pragma once

#include <uthash.h>

#include "transport.h"
#include "message.h"

struct kb_call_entry_s
{
    int id;
    void (*callback)(kb_message_t *message);
    UT_hash_handle hh;
};

typedef struct kb_call_entry_s kb_call_entry_t;

struct kb_call_registry_s
{
    kb_call_entry_t *entries;
};

typedef struct kb_call_registry_s kb_call_registry_t;

struct kb_rpc_s
{
    kb_transport_t *transport;
    kb_call_registry_t registry;
};

typedef struct kb_rpc_s kb_rpc_t;

kb_rpc_t *rpc_init(kb_transport_t *transport);
void handle_response(kb_rpc_t *rpc, kb_message_t *message);
void rpc_register(kb_rpc_t *rpc, int id, void (*callback)(kb_message_t *message));
void rpc_destroy(kb_rpc_t *rpc);
