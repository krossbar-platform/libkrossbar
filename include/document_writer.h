#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <log4c/category.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct kb_document_writer_s kb_document_writer_t;

kb_document_writer_t *doc_writer_create(uint8_t *data, size_t size, log4c_category_t *logger);
void doc_writer_destroy(kb_document_writer_t *writer);

size_t doc_writer_data_size(kb_document_writer_t *writer);

bool doc_writer_append_binary(kb_document_writer_t *writer, const char *key,
    const uint8_t *binary, uint32_t length);
bool doc_writer_append_utf8(kb_document_writer_t *writer, const char *key,
    const char *value, uint32_t length);
bool doc_writer_append_null(kb_document_writer_t *writer, const char *key);
bool doc_writer_append_bool(kb_document_writer_t *writer, const char *key, bool value);
bool doc_writer_append_double(kb_document_writer_t *writer, const char *key, double value);
bool doc_writer_append_int32(kb_document_writer_t *writer, const char *key, int32_t value);
bool doc_writer_append_int64(kb_document_writer_t *writer, const char *key, int64_t value);

kb_document_writer_t *doc_writer_append_document(kb_document_writer_t *writer, const char *key);
bool doc_writer_finish_document(kb_document_writer_t *writer, kb_document_writer_t *);

#ifdef __cplusplus
} // extern "C"
#endif
