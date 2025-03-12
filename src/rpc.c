#include "rpc.h"

uint64_t next_id(kb_rpc_t *rpc)
{
    return atomic_fetch_add(&rpc->id_counter, 1);
}

kb_rpc_t *rpc_init(kb_transport_t *transport)
{
    kb_rpc_t *rpc = calloc(1, sizeof(kb_rpc_t));
    if (rpc == NULL)
    {
        return NULL;
    }

    rpc->transport = transport;

    return rpc;
}

kb_message_writer_t *message(kb_rpc_t *rpc)
{
    return transport_message_init(rpc->transport);
}

kb_message_writer_t *call(kb_rpc_t *rpc, void (*callback)(kb_message_t *message))
{
    kb_message_writer_t *writer = transport_message_init(rpc->transport);

    return wrap_transport_message(rpc, writer, KB_MESSAGE_TYPE_CALL, callback);
}

kb_message_writer_t *subscribe(kb_rpc_t *rpc, void (*callback)(kb_message_t *message))
{
    kb_message_writer_t *writer = transport_message_init(rpc->transport);

    return wrap_transport_message(rpc, writer, KB_MESSAGE_TYPE_SUBSCRIPTION, callback);
}

kb_message_writer_t *wrap_transport_message(kb_rpc_t *rpc, kb_message_writer_t *writer, kb_message_type_t type, void (*callback)(kb_message_t *message))
{
    kb_rpc_message_t *message = malloc(sizeof(kb_rpc_message_t));

    if (message == NULL)
    {
        return NULL;
    }

    uint64_t id = next_id(rpc);

    message->rpc = rpc;
    message->transport_writer = writer;
    message->id = id;
    message->type = type;
    message->callback = callback;

    message->base.data_writer = writer->data_writer;
    message->base.logger = writer->logger;
    message->base.send = rpc_message_send;
    message->base.cancel = rpc_message_cancel;

    message_write_u64(message->transport_writer, id);

    return (kb_message_writer_t *)message;
}

int rpc_message_send(kb_message_writer_t *writer)
{
    kb_rpc_message_t *message = (kb_rpc_message_t *)writer;
    kb_rpc_t *rpc = message->rpc;

    int result = message_send(message->transport_writer);

    if (result != 0)
    {
        return result;
    }

    kb_call_entry_t *entry = malloc(sizeof(kb_call_entry_t));
    entry->id = message->id;
    entry->callback = message->callback;

    if (message->type == KB_MESSAGE_TYPE_CALL)
    {
        HASH_ADD_INT(rpc->call_registry.entries, id, entry);
    }
    else if (message->type == KB_MESSAGE_TYPE_SUBSCRIPTION)
    {
        HASH_ADD_INT(rpc->subs_registry.entries, id, entry);
    }

    return result;
}

void rpc_message_cancel(kb_message_writer_t *writer)
{
    return message_cancel(writer);
}

kb_message_t *handle_incoming_message(kb_rpc_t *rpc, kb_message_t *message)
{
    kb_call_entry_t *entry = NULL;

    // Look up for a call
    {
    }
    // Look up for a subscription
    {
    }
}

void rpc_destroy(kb_rpc_t *rpc)
{
    kb_call_entry_t *entry, *tmp;

    HASH_ITER(hh, rpc->call_registry.entries, entry, tmp)
    {
        HASH_DEL(rpc->call_registry.entries, entry);
        free(entry);
    }

    HASH_ITER(hh, rpc->subs_registry.entries, entry, tmp)
    {
        HASH_DEL(rpc->subs_registry.entries, entry);
        free(entry);
    }

    free(rpc);
}
