#include "message_writer.h"

#include <assert.h>

static void *kb_bson_realloc(void *mem, size_t num_bytes, void *ctx)
{
    return NULL;
}

void message_writer_init(kb_message_writer_t *writer, uint8_t *data, size_t size, log4c_category_t *logger)
{
    uint8_t bson_header[5] = {5, 0, 0, 0, 0};
    memcpy(data, bson_header, sizeof(bson_header));
    writer->data_writer = bson_new_from_buffer(&data, &size, kb_bson_realloc, NULL);
    if (writer->data_writer == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON writer\n");
        return;
    }

    writer->logger = logger;
    writer->buffer = data;
}

bson_t *message_writer_get_document(kb_message_writer_t *writer)
{
    assert(writer != NULL);

    return writer->data_writer;
}

size_t message_writer_size(kb_message_writer_t *writer)
{
    return writer->data_writer->len;
}

int message_send(kb_message_writer_t *writer)
{
    if (writer == NULL || writer->send == NULL)
    {
        return 1;
    }

    // Keep writer because we want to free self memory
    int result = writer->send(writer);

    bson_destroy(writer->data_writer);

    return result;
}

void message_cancel(kb_message_writer_t *writer)
{
    writer->cancel(writer);

    bson_destroy(writer->data_writer);
}
