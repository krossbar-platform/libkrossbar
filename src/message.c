#include "message.h"

#include <assert.h>
#include <stdbool.h>
#include <mpack.h>

void message_init(kb_message_t *message, void *data, size_t size)
{
    assert(message != NULL);
    assert(data != NULL);
    assert(size > 0);

    message->data_reader = malloc(sizeof(mpack_reader_t));
    mpack_reader_init_data(message->data_reader, data, size);
    message->destroy = NULL;
}

void message_destroy(kb_message_t *message)
{
    if (message == NULL)
    {
        return;
    }

    mpack_reader_destroy(message->data_reader);
    free(message->data_reader);

    if (message->destroy != NULL) {
        message->destroy(message);
    }
}

kb_message_tag_t message_read_tag(kb_message_t *message)
{
    return mpack_read_tag(message->data_reader);
}

kb_message_tag_t message_peek_tag(kb_message_t *message)
{
    return mpack_peek_tag(message->data_reader);
}

const char *message_read_bytes(kb_message_t *message, size_t count)
{
    return mpack_read_bytes_inplace(message->data_reader, count);
}