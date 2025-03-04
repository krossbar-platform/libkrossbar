#include "message_writer_uds.h"

#include <assert.h>

kb_message_writer_uds_t *message_writer_uds_init(kb_transport_uds_t *transport,
                                                 char *buffer,
                                                 size_t buffer_size)
{
    assert(transport != NULL);
    assert(buffer != NULL);
    assert(buffer_size > 0);

    kb_message_writer_uds_t *writer = malloc(sizeof(kb_message_writer_uds_t));
    writer->transport = transport;

    message_writer_init(&writer->base, buffer, buffer_size, transport->base.logger);
    writer->base.send = message_writer_uds_send;

    return writer;
}

int message_writer_uds_send(kb_message_writer_t *writer)
{
    if (writer == NULL)
    {
        return 1;
    }

    kb_message_writer_uds_t *self = (kb_message_writer_uds_t *)writer;

    transport_uds_message_send(&self->transport->base, writer);
    free(writer);

    return 0;
}