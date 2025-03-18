#pragma once

#include <uthash.h>
#include <stdatomic.h>

#include "transport.h"
#include "message.h"

enum kb_message_type_e
{
    KB_MESSAGE_TYPE_MESSAGE = 0,
    KB_MESSAGE_TYPE_CALL = 1,
    KB_MESSAGE_TYPE_SUBSCRIPTION = 2,
    KB_MESSAGE_TYPE_RESPONSE = 3,
};

typedef enum kb_message_type_e kb_message_type_t;

struct kb_call_entry_s
{
    int id;
    kb_message_type_t type;
    void (*callback)(kb_message_t *message);
    void *context;
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
    kb_call_registry_t calls_registry;
    kb_transport_t *transport;
    log4c_category_t *logger;
    atomic_uint_fast64_t id_counter;
};

typedef struct kb_rpc_s kb_rpc_t;

struct kb_rpc_message_writer_s
{
    kb_message_writer_t base;
    kb_message_writer_t *transport_writer;
    uint64_t id;
    kb_message_type_t type;
    void (*callback)(kb_message_t *message);
    void *context;
    kb_rpc_t *rpc;
};

typedef struct kb_rpc_message_writer_s kb_rpc_message_writer_t;

struct kb_rpc_message_s
{
    kb_message_t *message;
    kb_message_type_t type;
};

typedef struct kb_rpc_message_s kb_rpc_message_t;

uint64_t next_id(kb_rpc_t *rpc);

kb_rpc_t *rpc_init(kb_transport_t *transport, log4c_category_t *logger);
void rpc_destroy(kb_rpc_t *rpc);

kb_message_writer_t *rpc_message(kb_rpc_t *rpc);
kb_message_writer_t *rpc_call(kb_rpc_t *rpc, void (*callback)(kb_message_t *message), void *context);
kb_message_writer_t *rpc_subscribe(kb_rpc_t *rpc, void (*callback)(kb_message_t *message), void *context);

kb_message_writer_t *wrap_transport_message(kb_rpc_t *rpc, kb_message_writer_t *writer,
                                            kb_message_type_t type, void (*callback)(kb_message_t *message));
int rpc_message_send(kb_message_writer_t *writer);
void rpc_message_cancel(kb_message_writer_t *writer);
void rpc_message_release(kb_rpc_message_t *message);

kb_rpc_message_t *rpc_handle_incoming_message(kb_rpc_t *rpc, kb_message_t *message);
void rpc_write_message_header(kb_message_writer_t *message, uint64_t id, kb_message_type_t type);
