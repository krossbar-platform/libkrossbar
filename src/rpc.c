#include "rpc.h"

#include "message.h"

uint64_t next_id(kb_rpc_t *rpc)
{
    return atomic_fetch_add(&rpc->id_counter, 1);
}

kb_rpc_t *rpc_init(kb_transport_t *transport, log4c_category_t *logger)
{
    kb_rpc_t *rpc = malloc(sizeof(kb_rpc_t));
    if (rpc == NULL)
    {
        return NULL;
    }

    rpc->transport = transport;
    rpc->logger = logger;
    rpc->calls_registry.entries = NULL;
    rpc->id_counter = 1;

    return rpc;
}

kb_message_writer_t *rpc_message(kb_rpc_t *rpc)
{
    kb_message_writer_t *writer = transport_message_init(rpc->transport);

    if (writer)
    {
        rpc_write_message_header(writer, next_id(rpc), KB_MESSAGE_TYPE_MESSAGE);
    }

    return writer;
}

kb_message_writer_t *rpc_call(kb_rpc_t *rpc, void (*callback)(kb_message_t *, void *), void *context)
{
    kb_message_writer_t *writer = transport_message_init(rpc->transport);

    return rpc_wrap_transport_message(rpc, writer, KB_MESSAGE_TYPE_CALL, callback, context);
}

kb_message_writer_t *rpc_subscribe(kb_rpc_t *rpc, void (*callback)(kb_message_t *, void *), void *context)
{
    kb_message_writer_t *writer = transport_message_init(rpc->transport);

    return rpc_wrap_transport_message(rpc, writer, KB_MESSAGE_TYPE_SUBSCRIPTION, callback, context);
}

kb_message_writer_t *rpc_wrap_transport_message(kb_rpc_t *rpc, kb_message_writer_t *writer, kb_message_type_t type, void (*callback)(kb_message_t *, void *), void *context)
{
    if (writer == NULL)
    {
        return NULL;
    }

    kb_rpc_message_writer_t *message = malloc(sizeof(kb_rpc_message_writer_t));

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
    message->context = context;

    message->base.data_writer = writer->data_writer;
    message->base.logger = writer->logger;
    message->base.send = rpc_message_send;
    message->base.cancel = rpc_message_cancel;

    rpc_write_message_header(writer, id, type);

    return &message->base;
}

int rpc_message_send(kb_message_writer_t *writer)
{
    kb_rpc_message_writer_t *message = (kb_rpc_message_writer_t *)writer;
    kb_rpc_t *rpc = message->rpc;

    int result = message->transport_writer->send(message->transport_writer);

    if (result != 0)
    {
        return result;
    }

    kb_call_entry_t *entry = malloc(sizeof(kb_call_entry_t));
    entry->id = message->id;
    entry->callback = message->callback;
    entry->context = message->context;
    entry->type = message->type;

    HASH_ADD_INT(rpc->calls_registry.entries, id, entry);

    log4c_category_log(rpc->logger, LOG4C_PRIORITY_DEBUG, "Sending new message with id `%ld` of type `%d`", message->id, message->type);

    free(message);

    return result;
}

void rpc_message_cancel(kb_message_writer_t *writer)
{
    return message_cancel(writer);
}

kb_message_t *rpc_message_body(kb_rpc_message_t *message)
{
    return message->message;
}

kb_message_writer_t *rpc_message_respond(kb_rpc_message_t *message)
{
    kb_message_writer_t *writer = transport_message_init(message->rpc->transport);

    if (writer)
    {
        rpc_write_message_header(writer, message->id, KB_MESSAGE_TYPE_RESPONSE);
    }

    return writer;
}

void rpc_message_release(kb_rpc_message_t *message)
{
    message_destroy(message->message);
    free(message);
}

kb_rpc_message_t *rpc_handle_incoming_message(kb_rpc_t *rpc, kb_message_t *message)
{
    kb_call_entry_t *entry = NULL;

    kb_message_tag_t id_tag = message_read_tag(message);
    if (id_tag.type != mpack_type_uint)
    {
        log4c_category_log(rpc->logger, LOG4C_PRIORITY_ERROR, "Invalid mesage id type: %d", id_tag.type);
        return NULL;
    }

    kb_message_tag_t type_tag = message_read_tag(message);
    if (type_tag.type != mpack_type_uint)
    {
        log4c_category_log(rpc->logger, LOG4C_PRIORITY_ERROR, "Invalid mesage type type: %d", type_tag.type);
        return NULL;
    }

    uint64_t id = id_tag.v.u;
    kb_message_type_t type = type_tag.v.u;

    log4c_category_log(rpc->logger, LOG4C_PRIORITY_DEBUG, "Received new message with id `%ld` of type `%d`", id, type);

    if (type == KB_MESSAGE_TYPE_RESPONSE)
    {
        HASH_FIND_INT(rpc->calls_registry.entries, &id, entry);

        if (entry != NULL)
        {
            entry->callback(message, entry->context);
            if (entry->type == KB_MESSAGE_TYPE_CALL)
            {
                HASH_DEL(rpc->calls_registry.entries, entry);
                free(entry);
            }
        }

        message_destroy(message);

        return NULL;
    }
    else
    {
        kb_rpc_message_t *rpc_message = malloc(sizeof(kb_rpc_message_t));

        if (!rpc_message)
        {
            log4c_category_log(rpc->logger, LOG4C_PRIORITY_ERROR, "malloc failed");
            return NULL;
        }

        rpc_message->message = message;
        rpc_message->rpc = rpc;
        rpc_message->id = id;
        rpc_message->type = type;

        return rpc_message;
    }
}

void rpc_write_message_header(kb_message_writer_t *message, uint64_t id, kb_message_type_t type)
{
    message_write_u64(message, id);
    message_write_u8(message, type);
}

void rpc_destroy(kb_rpc_t *rpc)
{
    kb_call_entry_t *entry, *tmp;

    HASH_ITER(hh, rpc->calls_registry.entries, entry, tmp)
    {
        HASH_DEL(rpc->calls_registry.entries, entry);
        free(entry);
    }

    free(rpc);
}
