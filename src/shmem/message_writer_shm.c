#include "message_writer_shm.h"

#include <mpack.h>

kb_message_writer_shm_t *message_writer_shm_init(kb_transport_shm_t *transport,
                                                 kb_message_header_t *header,
                                                 char *buffer)
{
    kb_message_writer_shm_t *writer = malloc(sizeof(kb_message_writer_shm_t));
    writer->transport = transport;
    writer->header = header;

    writer->base.send = message_writer_shm_send;

    writer->base.writer = malloc(sizeof(mpack_writer_t));
    mpack_writer_init(writer->base.writer, buffer, header->size);

    return writer;
}

int message_writer_shm_send(kb_message_writer_t *writer)
{
    if (writer == NULL) {
        return 1;
    }

    kb_message_writer_shm_t *self = (kb_message_writer_shm_t*)writer;

    transport_shm_message_send(&self->transport->base, writer);
    mpack_writer_destroy(self->base.writer);
    free(self->base.writer);
    free(writer);

    return 0;
}