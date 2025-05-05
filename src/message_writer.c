#include "message_writer.h"

#include <assert.h>

#include "writers_private.h"

void message_writer_init(kb_message_writer_t *writer, uint8_t *data, size_t size, log4c_category_t *logger)
{
    uint8_t bson_header[5] = {5, 0, 0, 0, 0};
    memcpy(data, bson_header, sizeof(bson_header));
    writer->document_writer = doc_writer_from_buffer(data, size, logger);
    if (writer->document_writer == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to create document writer\n");
        return;
    }

    writer->logger = logger;
    writer->buffer = data;
}

kb_document_writer_t *message_writer_root(kb_message_writer_t *writer)
{
    assert(writer != NULL);

    return writer->document_writer;
}

size_t message_writer_size(kb_message_writer_t *writer)
{
    return doc_writer_data_size(writer->document_writer);
}

int message_send(kb_message_writer_t *writer)
{
    if (writer == NULL || writer->send == NULL)
    {
        return 1;
    }

    // Keep writer because we want to free self memory
    int result = writer->send(writer);

    return result;
}

void message_cancel(kb_message_writer_t *writer)
{
    writer->cancel(writer);

    doc_writer_destroy(writer->document_writer);
}
