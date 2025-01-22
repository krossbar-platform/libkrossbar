#include "message_shm.h"

#include <mpack.h>

kb_message_shm_t *message_shm_init(kb_transport_shm_t *transport,
                                   kb_message_header_t *header,
                                   char *buffer)
{
    kb_message_shm_t *message = calloc(1, sizeof(kb_message_shm_t));
    message->transport = transport;
    message->header = header;

    message->base.reader = malloc(sizeof(mpack_reader_t));
    message->base.destroy = message_shm_clean;
    mpack_reader_init_data(message->base.reader, buffer, header->size);

    return message;
}

int message_shm_clean(kb_message_t *message)
{
    if (message == NULL)
    {
        return 1;
    }

    kb_message_shm_t *self = (kb_message_shm_t *)message;

    transport_shm_message_release(&self->transport, message);
    mpack_reader_destroy(message->reader);
    free(message->reader);
    free(message);

    return 0;
}