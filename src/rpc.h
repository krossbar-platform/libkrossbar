#pragma once

#include <uthash.h>
#include <stdatomic.h>

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
    kb_call_registry_t call_registry;
    kb_call_registry_t subs_registry;
    kb_transport_t *transport;
    atomic_uint_fast64_t id_counter;
};

typedef struct kb_rpc_s kb_rpc_t;

enum kb_message_type_e
{
    KB_MESSAGE_TYPE_CALL = 0,
    KB_MESSAGE_TYPE_SUBSCRIPTION = 1,
    KB_MESSAGE_TYPE_RESPONSE = 2,
};

typedef enum kb_message_type_e kb_message_type_t;

struct kb_rpc_message_s
{
    kb_message_writer_t base;
    kb_message_writer_t *transport_writer;
    uint64_t id;
    kb_message_type_t type;
    void (*callback)(kb_message_t *message);
    kb_rpc_t *rpc;
};

typedef struct kb_rpc_message_s kb_rpc_message_t;

uint64_t next_id(kb_rpc_t *rpc);

kb_rpc_t *rpc_init(kb_transport_t *transport);
void rpc_destroy(kb_rpc_t *rpc);

kb_message_writer_t *message(kb_rpc_t *rpc);
kb_message_writer_t *call(kb_rpc_t *rpc, void (*callback)(kb_message_t *message));
kb_message_writer_t *subscribe(kb_rpc_t *rpc, void (*callback)(kb_message_t *message));

kb_message_writer_t *wrap_transport_message(kb_rpc_t *rpc, kb_message_writer_t *writer,
                                            kb_message_type_t type, void (*callback)(kb_message_t *message));
int rpc_message_send(kb_message_writer_t *writer);
void rpc_message_cancel(kb_message_writer_t *writer);

kb_message_t *handle_incoming_message(kb_rpc_t *rpc, kb_message_t *message);
