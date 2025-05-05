#pragma once

#include <bson.h>
#include <log4c/category.h>

struct kb_document_s
{
    bson_iter_t iter;
    log4c_category_t *logger;
};
typedef struct kb_document_s kb_document_t;

kb_document_t *document_from_data(uint8_t *data, size_t size, log4c_category_t *logger);
bool document_destroy(kb_document_t *document);

