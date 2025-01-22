#include "message_writer.h"

#include <mpack.h>

#define WRITER_FUNC_0(name)                          \
void message_##name(kb_message_writer_t *writer)     \
{                                                    \
    mpack_##name(writer->writer);                   \
}

#define WRITER_FUNC_1(name, val_type)                \
void message_##name(kb_message_writer_t *writer, val_type value) \
{                                                    \
    mpack_##name(writer->writer, value);            \
}

#define WRITER_FUNC_2(name, val1_type, val2_type)           \
void message_##name(kb_message_writer_t *writer, val1_type value1, val2_type value2) \
{                                                           \
    mpack_##name(writer->writer, value1, value2);          \
}


// kb_writer_error_t writer_error(kb_message_writer_t* writer) {
//     return mpack_writer_error(&writer->writer);
// }

size_t writer_message_size(kb_message_writer_t* writer)
{
    return mpack_writer_buffer_used(writer->writer);
}

int message_send(kb_message_writer_t *writer)
{
    return writer->send(writer);
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