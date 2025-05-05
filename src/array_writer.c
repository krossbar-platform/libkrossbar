#include "array_writer.h"

#include <assert.h>

#include <bson.h>

#include "writers_private.h"

kb_array_writer_t *arr_writer_create(kb_document_writer_t *document, const char *key, log4c_category_t *logger)
{
    kb_array_writer_t *writer = malloc(sizeof(kb_array_writer_t));
    if (writer == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for array writer\n");
        return NULL;
    }
    writer->parent_document = document->bson;
    writer->parent_array = document->parent_array;
    writer->logger = logger;

    if (!bson_append_array_builder_begin(writer->parent_document, key, -1, &writer->builder))
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON array builder\n");
        free(writer);
        return NULL;
    }

    return writer;
}

bool arr_writer_finish(kb_array_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->parent_document != NULL || writer->parent_array != NULL);
    assert(writer->builder != NULL);

    if (writer->parent_document)
    {
        if (!bson_append_array_builder_end(writer->parent_document, writer->builder))
        {
            log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to finish BSON array builder\n");
            return false;
        }
    }
    else if (writer->parent_array)
    {
        if (!bson_array_builder_append_array_builder_end(writer->parent_array, writer->builder))
        {
            log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to append BSON array\n");
            return false;
        }
    }

    free(writer);

    return true;
}

bool arr_writer_append_binary(kb_array_writer_t *writer, const char *key,
                              const uint8_t *binary, uint32_t length)
{
    assert(writer != NULL);
    assert(key != NULL);
    assert(binary != NULL);

    return bson_array_builder_append_binary(writer->builder, BSON_SUBTYPE_BINARY, binary, length);
}

bool arr_writer_append_utf8(kb_array_writer_t *writer, const char *key,
                            const char *value, uint32_t length)
{
    assert(writer != NULL);
    assert(key != NULL);
    assert(value != NULL);

    return bson_array_builder_append_utf8(writer->builder, value, length);
}

bool arr_writer_append_null(kb_array_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(key != NULL);

    return bson_array_builder_append_null(writer->builder);
}

bool arr_writer_append_bool(kb_array_writer_t *writer, const char *key, bool value)
{
    assert(writer != NULL);
    assert(key != NULL);

    return bson_array_builder_append_bool(writer->builder, value);
}

bool arr_writer_append_double(kb_array_writer_t *writer, const char *key, double value)
{
    assert(writer != NULL);
    assert(key != NULL);

    return bson_array_builder_append_double(writer->builder, value);
}

bool arr_writer_append_int32(kb_array_writer_t *writer, const char *key, int32_t value)
{
    assert(writer != NULL);
    assert(key != NULL);

    return bson_array_builder_append_int32(writer->builder, value);
}

bool arr_writer_append_int64(kb_array_writer_t *writer, const char *key, int64_t value)
{
    assert(writer != NULL);
    assert(key != NULL);

    return bson_array_builder_append_int64(writer->builder, value);
}

kb_document_writer_t *arr_writer_document_begin(kb_array_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(writer->builder != NULL);
    assert(key != NULL);

    kb_document_writer_t *sub_writer = malloc(sizeof(kb_document_writer_t));
    if (sub_writer == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for sub-document writer\n");
        return NULL;
    }

    sub_writer->logger = writer->logger;
    sub_writer->bson = bson_new();
    sub_writer->parent_array = writer->builder;
    if (sub_writer->bson == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON writer\n");
        free(sub_writer);
        return NULL;
    }

    if (!bson_array_builder_append_document_begin(writer->builder, sub_writer->bson))
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to init internal document\n");
        free(sub_writer);
        return NULL;
    }

    return sub_writer;
}

bool arr_writer_document_end(kb_document_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->bson != NULL);
    assert(writer->parent_array != NULL);

    if (writer->parent_array == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "No parent array to finish document\n");
        return false;
    }

    if (!bson_array_builder_append_document_end(writer->parent_array, writer->bson))
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to finish BSON document\n");
        return false;
    }
    free(writer);

    return true;
}

kb_array_writer_t *arr_writer_array_begin(kb_array_writer_t *writer, const char *key)
{
    assert(writer != NULL);
    assert(writer->builder != NULL);
    assert(key != NULL);

    kb_array_writer_t *sub_writer = malloc(sizeof(kb_array_writer_t));
    if (sub_writer == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for sub-array writer\n");
        return NULL;
    }

    sub_writer->logger = writer->logger;
    sub_writer->parent_array = writer->builder;
    if (sub_writer->parent_array == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to create BSON array builder\n");
        free(sub_writer);
        return NULL;
    }

    if (!bson_array_builder_append_array_builder_begin(writer->builder, &sub_writer->builder))
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to init internal array\n");
        free(sub_writer);
        return NULL;
    }

    return sub_writer;
}

bool arr_writer_array_end(kb_array_writer_t *writer)
{
    assert(writer != NULL);
    assert(writer->builder != NULL);
    assert(writer->parent_array != NULL);

    if (writer->parent_array == NULL)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "No parent array to finish array\n");
        return false;
    }

    if (!bson_array_builder_append_array_builder_end(writer->parent_array, writer->builder))
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Failed to finish BSON array\n");
        return false;
    }
    free(writer);

    return true;
}

