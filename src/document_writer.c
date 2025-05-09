#include "document_writer.h"

#include <assert.h>

#include <bson.h>

#include "writers_private.h"

static void *kb_bson_realloc(void *mem, size_t num_bytes, void *ctx)
{
    return NULL;
}

kb_document_writer_t *doc_writer_from_buffer(uint8_t *data, size_t size, log4c_category_t *logger)
{
    kb_document_writer_t *writer = malloc(sizeof(kb_document_writer_t));
    if (writer == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for document writer\n");
        return NULL;
    }
    writer->logger = logger;
    writer->parent_document = NULL;
    writer->parent_array = NULL;

    uint8_t bson_header[5] = {5, 0, 0, 0, 0};
    memcpy(data, bson_header, sizeof(bson_header));
    writer->bson = bson_new_from_buffer(&data, &size, kb_bson_realloc, NULL);
    if (writer->bson == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON writer\n");
        return NULL;
    }
}

bool doc_writer_destroy(kb_document_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->parent_document == NULL && writer->parent_array == NULL);

    bson_destroy(writer->bson);
    return true;
}

size_t doc_writer_data_size(kb_document_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);

    return (writer->bson)->len;
}

bool doc_writer_append_binary(kb_document_writer_t *writer, const char *key,
    const uint8_t *binary, uint32_t length)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);
    assert(binary != NULL);

    return bson_append_binary(writer->bson, key, -1, BSON_SUBTYPE_BINARY, binary, length);
}

bool doc_writer_append_utf8(kb_document_writer_t *writer, const char *key,
    const char *value, uint32_t length)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);
    assert(value != NULL);

    return bson_append_utf8(writer->bson, key, -1, value, length);
}

bool doc_writer_append_null(kb_document_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);

    return bson_append_null(writer->bson, key, -1);
}

bool doc_writer_append_bool(kb_document_writer_t *writer, const char *key, bool value)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);

    return bson_append_bool(writer->bson, key, -1, value);
}

bool doc_writer_append_double(kb_document_writer_t *writer, const char *key, double value)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);

    return bson_append_double(writer->bson, key, -1, value);
}

bool doc_writer_append_int32(kb_document_writer_t *writer, const char *key, int32_t value)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);

    return bson_append_int32(writer->bson, key, -1, value);
}

bool doc_writer_append_int64(kb_document_writer_t *writer, const char *key, int64_t value)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);

    return bson_append_int64(writer->bson, key, -1, value);
}

kb_document_writer_t *doc_writer_document_begin(kb_document_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);

    kb_document_writer_t *sub_writer = malloc(sizeof(kb_document_writer_t));
    if (sub_writer == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for sub-document writer\n");
        return NULL;
    }

    sub_writer->logger = writer->logger;
    sub_writer->bson = bson_new();
    sub_writer->parent_document = writer->bson;
    if (sub_writer->bson == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON writer\n");
        free(sub_writer);
        return NULL;
    }

    if (!bson_append_document_begin(sub_writer->parent_document, key, -1, sub_writer->bson))
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to init internal document\n");
        free(sub_writer);
        return NULL;
    }

    return sub_writer;
}
bool doc_writer_document_end(kb_document_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);

    if (!writer->parent_document)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Can't end root document\n");
        return false;
    }

    if (!bson_append_document_end(writer->parent_document, writer->bson))
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to append document end\n");
        return false;
    }

    return true;
}

kb_array_writer_t *doc_writer_array_begin(kb_document_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(key != NULL);

    return arr_writer_create(writer, key, writer->logger);
}

bool doc_writer_array_end(kb_array_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->builder != NULL);

    return arr_writer_finish(writer);
}
