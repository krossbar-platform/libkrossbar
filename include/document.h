#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <log4c/category.h>

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kb_document_s kb_document_t;
typedef struct kb_array_s kb_array_t;

bool document_has_field(kb_document_t *document, const char *key);
kb_value_t document_get_value(kb_document_t *document, const char *key);
kb_value_type_t document_value_type(kb_document_t *document, const char *key);

bool document_get_bool(kb_document_t *document, const char *key, bool *success);
int32_t document_get_int32(kb_document_t *document, const char *key, bool *success);
int64_t document_get_int64(kb_document_t *document, const char *key, bool *success);
double document_get_double(kb_document_t *document, const char *key, bool *success);
const char *document_get_utf8(kb_document_t *document, const char *key, size_t *size, bool *success);
const uint8_t *document_get_binary(kb_document_t *document, const char *key, size_t *size, bool *success);
kb_document_t *document_get_document(kb_document_t *document, const char *key, bool *success);
kb_array_t *document_get_array(kb_document_t *document, const char *key, bool *success);

#ifdef __cplusplus
} // extern "C"
#endif
