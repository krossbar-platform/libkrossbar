#include "message_writer.h"

#include <mpack.h>

#define WRITER_FUNC_0(name)                                        \
    kb_message_error_t message_##name(kb_message_writer_t *writer) \
    {                                                              \
        mpack_##name(writer->data_writer);                         \
        return mpack_writer_error(writer->data_writer);            \
    }

#define WRITER_FUNC_1(name, val_type)                                              \
    kb_message_error_t message_##name(kb_message_writer_t *writer, val_type value) \
    {                                                                              \
        mpack_##name(writer->data_writer, value);                                  \
        return mpack_writer_error(writer->data_writer);                            \
    }

#define WRITER_FUNC_2(name, val1_type, val2_type)                                                      \
    kb_message_error_t message_##name(kb_message_writer_t *writer, val1_type value1, val2_type value2) \
    {                                                                                                  \
        mpack_##name(writer->data_writer, value1, value2);                                             \
        return mpack_writer_error(writer->data_writer);                                                \
    }

void message_writer_init(kb_message_writer_t *writer, void *data, size_t size, log4c_category_t *logger)
{
    writer->data_writer = malloc(sizeof(mpack_writer_t));
    writer->logger = logger;
    mpack_writer_init(writer->data_writer, data, size);
}

size_t writer_message_size(kb_message_writer_t* writer)
{
    return mpack_writer_buffer_used(writer->data_writer);
}

int message_send(kb_message_writer_t *writer)
{
    if (writer == NULL || writer->send == NULL)
    {
        return 1;
    }

    if (mpack_writer_error(writer->data_writer) != mpack_ok)
    {
        log4c_category_log(writer->logger, LOG4C_PRIORITY_ERROR, "Error writing message: %s\n", mpack_error_to_string(mpack_writer_error(writer->data_writer)));
        return 1;
    }

    // Keep writer because we want to free self memory
    mpack_writer_t *data_writer = writer->data_writer;
    int result = writer->send(writer);

    mpack_writer_destroy(data_writer);
    free(data_writer);

    return result;
}

WRITER_FUNC_1(write_i8, int8_t);
WRITER_FUNC_1(write_i16, int16_t);
WRITER_FUNC_1(write_i32, int32_t);
WRITER_FUNC_1(write_i64, int64_t);
WRITER_FUNC_1(write_int, int64_t);
WRITER_FUNC_1(write_u8, uint8_t);
WRITER_FUNC_1(write_u16, uint16_t);
WRITER_FUNC_1(write_u32, uint32_t);
WRITER_FUNC_1(write_u64, uint64_t);
WRITER_FUNC_1(write_uint, uint64_t);
WRITER_FUNC_1(write_float, float);
WRITER_FUNC_1(write_double, double);
WRITER_FUNC_1(write_bool, bool);
WRITER_FUNC_0(write_nil);
WRITER_FUNC_2(write_str, const char*, uint32_t);
WRITER_FUNC_2(write_utf8, const char*, uint32_t);
WRITER_FUNC_1(write_cstr, const char*);
WRITER_FUNC_1(write_cstr_or_nil, const char*);
WRITER_FUNC_1(write_utf8_cstr, const char*);
WRITER_FUNC_2(write_bin, const char*, uint32_t);

WRITER_FUNC_1(start_map, uint32_t);
WRITER_FUNC_0(finish_map);
WRITER_FUNC_0(build_map);
WRITER_FUNC_0(complete_map);

WRITER_FUNC_1(start_array, uint32_t);
WRITER_FUNC_0(finish_array);
WRITER_FUNC_0(build_array);
WRITER_FUNC_0(complete_array);