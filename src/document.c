#include "document.h"

#include <assert.h>

#include "readers_private.h"

kb_document_t *document_from_data(uint8_t *data, size_t size, log4c_category_t *logger)
{
    kb_document_t *document = malloc(sizeof(kb_document_t));
    if (document == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to allocate memory for document\n");
        return NULL;
    }

    document->logger = logger;

    if (!bson_iter_init_from_data(&document->iter, data, size))
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to initialize BSON iterator\n");
        free(document);
        return NULL;
    }

    return document;
}

bool document_destroy(kb_document_t *document)
{
    assert(document != NULL);

    free(document);
    return true;
}

bool document_has_field(kb_document_t *document, const char *key)
{
    assert(document != NULL);
    assert(key != NULL);

    return bson_iter_find(&document->iter, key);
}

kb_value_t document_get_value(kb_document_t *document, const char *key)
{
    assert(document != NULL);
    assert(key != NULL);

    kb_value_t value;
    if (!bson_iter_find(&document->iter, key))
    {
        value.type = KB_VALUE_DOES_NOT_EXIST;
        return value;
    }

    bson_type_t bson_type = bson_iter_type(&document->iter);
    switch (bson_type)
    {
        case BSON_TYPE_NULL:
            value.type = KB_VALUE_TYPE_NULL;
            break;
        case BSON_TYPE_BOOL:
            value.type = KB_VALUE_TYPE_BOOL;
            value.value.boolean = bson_iter_bool(&document->iter);
            break;
        case BSON_TYPE_INT32:
            value.type = KB_VALUE_TYPE_INT32;
            value.value.int32 = bson_iter_int32(&document->iter);
            break;
        case BSON_TYPE_INT64:
            value.type = KB_VALUE_TYPE_INT64;
            value.value.int64 = bson_iter_int64(&document->iter);
            break;
        case BSON_TYPE_DOUBLE:
            value.type = KB_VALUE_TYPE_DOUBLE;
            value.value.double_value = bson_iter_double(&document->iter);
            break;
        case BSON_TYPE_UTF8:
            value.type = KB_VALUE_TYPE_STRING;
            value.value.utf8.str = (char *)bson_iter_utf8(&document->iter, &value.value.utf8.len);
            break;
        case BSON_TYPE_BINARY:
            value.type = KB_VALUE_TYPE_BINARY;
            bson_iter_binary(&document->iter, NULL, &value.value.binary.data_len, &value.value.binary.data);
            break;
        case BSON_TYPE_DOCUMENT:
            value.type = KB_VALUE_TYPE_DOCUMENT;
            // Handle document type
            break;
        case BSON_TYPE_ARRAY:
            value.type = KB_VALUE_TYPE_ARRAY;
            // Handle array type
            break;
        default:
            value.type = KB_VALUE_DOES_NOT_EXIST;
    }

    return value;
}

kb_value_type_t document_value_type(kb_document_t *document, const char *key)
{
    assert(document != NULL);
    assert(key != NULL);

    if (!bson_iter_find(&document->iter, key))
    {
        return KB_VALUE_DOES_NOT_EXIST;
    }

    bson_type_t bson_type = bson_iter_type(&document->iter);
    switch (bson_type)
    {
        case BSON_TYPE_NULL:
            return KB_VALUE_TYPE_NULL;
        case BSON_TYPE_BOOL:
            return KB_VALUE_TYPE_BOOL;
        case BSON_TYPE_INT32:
            return KB_VALUE_TYPE_INT32;
        case BSON_TYPE_INT64:
            return KB_VALUE_TYPE_INT64;
        case BSON_TYPE_DOUBLE:
            return KB_VALUE_TYPE_DOUBLE;
        case BSON_TYPE_UTF8:
            return KB_VALUE_TYPE_STRING;
        case BSON_TYPE_BINARY:
            return KB_VALUE_TYPE_BINARY;
        case BSON_TYPE_DOCUMENT:
            return KB_VALUE_TYPE_DOCUMENT;
        case BSON_TYPE_ARRAY:
            return KB_VALUE_TYPE_ARRAY;
        default:
            return KB_VALUE_DOES_NOT_EXIST;
    }
}

bool document_get_bool(kb_document_t *document, const char *key, bool *success)
{
    assert(document != NULL);
    assert(key != NULL);
    assert(success != NULL);

    if (!bson_iter_find(&document->iter, key))
    {
        *success = false;
        return false;
    }

    if (bson_iter_type(&document->iter) != BSON_TYPE_BOOL)
    {
        *success = false;
        return false;
    }

    *success = true;
    return bson_iter_bool(&document->iter);
}

int32_t document_get_int32(kb_document_t *document, const char *key, bool *success)
{
    assert(document != NULL);
    assert(key != NULL);
    assert(success != NULL);

    if (!bson_iter_find(&document->iter, key))
    {
        *success = false;
        return 0;
    }

    if (bson_iter_type(&document->iter) != BSON_TYPE_INT32)
    {
        *success = false;
        return 0;
    }

    *success = true;
    return bson_iter_int32(&document->iter);
}

int64_t document_get_int64(kb_document_t *document, const char *key, bool *success)
{
    assert(document != NULL);
    assert(key != NULL);
    assert(success != NULL);

    if (!bson_iter_find(&document->iter, key))
    {
        *success = false;
        return 0;
    }

    if (bson_iter_type(&document->iter) != BSON_TYPE_INT64)
    {
        *success = false;
        return 0;
    }

    *success = true;
    return bson_iter_int64(&document->iter);
}

double document_get_double(kb_document_t *document, const char *key, bool *success)
{
    assert(document != NULL);
    assert(key != NULL);
    assert(success != NULL);

    if (!bson_iter_find(&document->iter, key))
    {
        *success = false;
        return 0.0;
    }

    if (bson_iter_type(&document->iter) != BSON_TYPE_DOUBLE)
    {
        *success = false;
        return 0.0;
    }

    *success = true;
    return bson_iter_double(&document->iter);
}

const char *document_get_utf8(kb_document_t *document, const char *key, size_t *size, bool *success)
{
    assert(document != NULL);
    assert(key != NULL);
    assert(size != NULL);
    assert(success != NULL);

    if (!bson_iter_find(&document->iter, key))
    {
        *success = false;
        return NULL;
    }

    if (bson_iter_type(&document->iter) != BSON_TYPE_UTF8)
    {
        *success = false;
        return NULL;
    }

    *success = true;
    return bson_iter_utf8(&document->iter, (uint32_t *)size);
}

const uint8_t *document_get_binary(kb_document_t *document, const char *key, size_t *size, bool *success)
{
    assert(document != NULL);
    assert(key != NULL);
    assert(size != NULL);
    assert(success != NULL);

    if (!bson_iter_find(&document->iter, key))
    {
        *success = false;
        return NULL;
    }

    if (bson_iter_type(&document->iter) != BSON_TYPE_BINARY)
    {
        *success = false;
        return NULL;
    }

    *success = true;

    bson_subtype_t subtype;
    const uint8_t *data = NULL;
    bson_iter_binary(&document->iter, &subtype, (uint32_t *)size, &data);
    return data;
}

kb_document_t *document_get_document(kb_document_t *document, const char *key, bool *success);
kb_array_t *document_get_array(kb_document_t *document, const char *key, bool *success);