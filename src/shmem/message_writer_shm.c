#include "message_writer_shm.h"

#include <assert.h>

#include <mpack.h>

kb_message_writer_shm_t *message_writer_shm_init(kb_transport_shm_t *transport,
                                                 kb_message_header_t *header,
                                                 char *buffer)
{
    assert(transport != NULL);
    assert(header != NULL);
    assert(buffer != NULL);

    kb_message_writer_shm_t *writer = malloc(sizeof(kb_message_writer_shm_t));
    writer->transport = transport;
    writer->header = header;

    message_writer_init(&writer->base, buffer, header->size, transport->base.logger);
    writer->base.send = message_writer_shm_send;
    writer->base.cancel = message_writer_shm_cancel;

    return writer;
}

int message_writer_shm_send(kb_message_writer_t *writer)
{
    if (writer == NULL) {
        return 1;
    }

    kb_message_writer_shm_t *self = (kb_message_writer_shm_t*)writer;

    transport_shm_message_send(&self->transport->base, writer);
    free(writer);

    return 0;
}

void message_writer_shm_cancel(kb_message_writer_t *writer)
{
    free(writer);
}
