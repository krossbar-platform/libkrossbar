#pragma once

#include <stdint.h>

enum kb_value_type_e
{
    KB_VALUE_TYPE_NULL,
    KB_VALUE_TYPE_BOOL,
    KB_VALUE_TYPE_INT32,
    KB_VALUE_TYPE_INT64,
    KB_VALUE_TYPE_DOUBLE,
    KB_VALUE_TYPE_STRING,
    KB_VALUE_TYPE_BINARY,
    KB_VALUE_TYPE_DOCUMENT,
    KB_VALUE_TYPE_ARRAY,
    KB_VALUE_DOES_NOT_EXIST,
};
typedef enum kb_value_type_e kb_value_type_t;

struct kb_value_s
{
    kb_value_type_t type;
    union
    {
        bool boolean;
        int32_t int32;
        int64_t int64;
        double double_value;
        struct
        {
            uint32_t len;
            char const *str;
        } utf8;
        struct {
            uint32_t data_len;
            uint8_t const *data;
        } binary;
        struct kb_document_s *document;
        struct kb_array_s *array;
    } value;
};
typedef struct kb_value_s kb_value_t;
