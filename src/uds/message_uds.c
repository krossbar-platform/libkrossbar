#include "message_uds.h"

#include <assert.h>

kb_message_uds_t *message_uds_init(kb_transport_uds_t *transport,
                                   char *buffer,
                                   size_t size)
{
    assert(transport != NULL);
    assert(buffer != NULL);
    assert(size > 0);

    kb_message_uds_t *message = malloc(sizeof(kb_message_uds_t));
    message->transport = transport;

    message_init(&message->base, buffer, size);
    message->base.destroy = message_uds_clean;

    return message;
}

int message_uds_clean(kb_message_t *message)
{
    if (message == NULL)
    {
        return 1;
    }

    kb_message_uds_t *self = (kb_message_uds_t *)message;

    transport_uds_message_release(&self->transport->base, message);
    free(self);

    return 0;
}