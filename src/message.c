#include "message.h"

#include <assert.h>
#include <stdbool.h>

#include <bson.h>

typedef bson_reader_t kb_data_reader_t;
typedef bson_t kb_data_document_t;

void message_init(kb_message_t *message, uint8_t *data, size_t size)
{
    assert(message != NULL);
    assert(data != NULL);
    assert(size > 0);

    message->document = bson_new();
    if (message->document == NULL)
    {
        return;
    }

    bson_init_static(message->document, data, size);
}

void message_destroy(kb_message_t *message)
{
    if (message == NULL)
    {
        return;
    }

    bson_destroy(message->document);

    if (message->destroy != NULL) {
        message->destroy(message);
    }
}

bson_t *message_get_document(kb_message_t *message)
{
    assert(message != NULL);
    assert(message->document != NULL);

    return message->document;
}
