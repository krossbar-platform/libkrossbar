#include "rpc.h"

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

void handle_response(kb_rpc_t *rpc, kb_message_t *message)
{
    kb_call_entry_t *entry = NULL;
    HASH_FIND_INT(rpc->registry.entries, &message->id, entry);

    if (entry != NULL)
    {
        entry->callback(message);
    }

    HASH_DEL(rpc->registry.entries, entry);
}

void rpc_register(kb_rpc_t *rpc, int id, void (*callback)(kb_message_t *message))
{
    kb_call_entry_t *entry = calloc(1, sizeof(kb_call_entry_t));
    entry->id = id;
    entry->callback = callback;

    HASH_ADD_INT(rpc->registry.entries, id, entry);
}

void rpc_destroy(kb_rpc_t *rpc)
{
    kb_call_entry_t *entry, *tmp;

    HASH_ITER(hh, rpc->registry.entries, entry, tmp)
    {
        HASH_DEL(rpc->registry.entries, entry);
        free(entry);
    }

    free(rpc);
}