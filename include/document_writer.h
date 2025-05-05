#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <log4c/category.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct kb_document_writer_s kb_document_writer_t;
typedef struct kb_array_writer_s kb_array_writer_t;

bool doc_writer_append_binary(kb_document_writer_t *writer, const char *key,
    const uint8_t *binary, uint32_t length);
bool doc_writer_append_utf8(kb_document_writer_t *writer, const char *key,
    const char *value, uint32_t length);
bool doc_writer_append_null(kb_document_writer_t *writer, const char *key);
bool doc_writer_append_bool(kb_document_writer_t *writer, const char *key, bool value);
bool doc_writer_append_double(kb_document_writer_t *writer, const char *key, double value);
bool doc_writer_append_int32(kb_document_writer_t *writer, const char *key, int32_t value);
bool doc_writer_append_int64(kb_document_writer_t *writer, const char *key, int64_t value);

kb_document_writer_t *doc_writer_document_begin(kb_document_writer_t *writer, const char *key);
bool doc_writer_document_end(kb_document_writer_t *writer);

kb_array_writer_t *doc_writer_array_begin(kb_document_writer_t *writer, const char *key);
bool doc_writer_array_end(kb_array_writer_t *writer);

#ifdef __cplusplus
} // extern "C"
#endif
