#include "message_shm.h"

#include <mpack.h>

kb_message_shm_t *message_shm_init(kb_transport_shm_t *transport,
                                   kb_message_header_t *header,
                                   char *buffer)
{
    kb_message_shm_t *message = calloc(1, sizeof(kb_message_shm_t));
    message->transport = transport;
    message->header = header;

    message_init(&message->base, buffer, header->size);
    message->base.destroy = message_shm_clean;

    return message;
}

int message_shm_clean(kb_message_t *message)
{
    if (message == NULL)
    {
        return 1;
    }

    kb_message_shm_t *self = (kb_message_shm_t *)message;

    transport_shm_message_release(&self->transport->base, message);
    free(self);

    return 0;
}