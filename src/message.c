#include "message.h"

#include <stdbool.h>
#include <mpack.h>

kb_message_t *message_init(void* data, size_t size)
{
    kb_message_t *message = calloc(1, sizeof(kb_message_t));
    mpack_reader_init_data(&message->reader, data, size);

    return message;
}

void mesage_destroy(kb_message_t *message)
{
    if (message == NULL)
    {
        return;
    }

    mpack_reader_destroy(&message->reader);

    if (message->destroy != NULL) {
        message->destroy(message);
    }

    free(message);
}

kb_message_tag_t message_read_tag(kb_message_t *message)
{
    return mpack_read_tag(&message->reader);
}

kb_message_tag_t message_peek_tag(kb_message_t *message)
{
    return mpack_peek_tag(&message->reader);
}