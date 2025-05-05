#include <bson.h>
#include <log4c/category.h>

struct kb_document_writer_s
{
    bson_t *bson;
    bson_t *parent_document;
    bson_array_builder_t *parent_array;
    log4c_category_t *logger;
};

struct kb_array_writer_s
{
    bson_array_builder_t *builder;
    bson_t *parent_document;
    bson_array_builder_t *parent_array;
    log4c_category_t *logger;
};

// Document
kb_document_writer_t *doc_writer_from_buffer(uint8_t *data, size_t size, log4c_category_t *logger);
bool doc_writer_destroy(kb_document_writer_t *writer);

size_t doc_writer_data_size(kb_document_writer_t *writer);

// Array
kb_array_writer_t *arr_writer_create(kb_document_writer_t *document, const char *key, log4c_category_t *logger);
bool arr_writer_finish(kb_array_writer_t *writer);