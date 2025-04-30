#include "document_writer.h"

#include <assert.h>

#include <bson.h>

struct kb_document_writer_s
{
    bson_t bson;
    log4c_category_t *logger;
};

static void *kb_bson_realloc(void *mem, size_t num_bytes, void *ctx)
{
    return NULL;
}

kb_document_writer_t *doc_writer_create(uint8_t *data, size_t size, log4c_category_t *logger)
{
    kb_document_writer_t *writer = malloc(sizeof(kb_document_writer_t));
    if (writer == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for document writer\n");
        return NULL;
    }
    writer->logger = logger;

    uint8_t bson_header[5] = {5, 0, 0, 0, 0};
    memcpy(data, bson_header, sizeof(bson_header));
    writer->document = (kb_document_t*)bson_new_from_buffer(&data, &size, kb_bson_realloc, NULL);
    if (writer->document == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON writer\n");
        return NULL;
    }
}

void doc_writer_destroy(kb_document_writer_t *writer)
{
    assert(writer != NULL);

    bson_destroy((bson_t*)writer->document);
}

size_t doc_writer_data_size(kb_document_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->document != NULL);

    return ((bson_t*)writer->document)->len;
}

bool doc_writer_append_binary(kb_document_writer_t *writer, const char *key,
    const uint8_t *binary, uint32_t length)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);
    assert(binary != NULL);

    return bson_append_binary((bson_t*)writer->document, key, -1, BSON_SUBTYPE_BINARY, binary, length);
}

bool doc_writer_append_utf8(kb_document_writer_t *writer, const char *key,
    const char *value, uint32_t length)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);
    assert(value != NULL);

    return bson_append_utf8((bson_t*)writer->document, key, -1, value, length);
}

bool doc_writer_append_null(kb_document_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);

    return bson_append_null((bson_t*)writer->document, key, -1);
}

bool doc_writer_append_bool(kb_document_writer_t *writer, const char *key, bool value)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);

    return bson_append_bool((bson_t*)writer->document, key, -1, value);
}

bool doc_writer_append_double(kb_document_writer_t *writer, const char *key, double value)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);

    return bson_append_double((bson_t*)writer->document, key, -1, value);
}

bool doc_writer_append_int32(kb_document_writer_t *writer, const char *key, int32_t value)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);

    return bson_append_int32((bson_t*)writer->document, key, -1, value);
}

bool doc_writer_append_int64(kb_document_writer_t *writer, const char *key, int64_t value)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);

    return bson_append_int64((bson_t*)writer->document, key, -1, value);
}

kb_document_writer_t *doc_writer_append_document(kb_document_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(writer->document != NULL);
    assert(key != NULL);

    kb_document_writer_t *sub_writer = malloc(sizeof(kb_document_writer_t));
    if (sub_writer == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for sub-document writer\n");
        return NULL;
    }

    sub_writer->logger = writer->logger;
    sub_writer->document = (kb_document_t*)bson_new_from_buffer(&writer->document, &writer->document, kb_bson_realloc, NULL);
    if (sub_writer->document == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON writer\n");
        free(sub_writer);
        return NULL;
    }

    bson_append_document_begin((bson_t*)writer->document, key, -1, (bson_t*)sub_writer->document);

    return sub_writer;
}
bool dw_append_document_end(kb_document_writer_t *writer, kb_document_writer_t *);

