#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct mpack_writer_t;

struct kb_message_writer_s {
    struct mpack_writer_t *writer;
    int (*send)(struct kb_message_writer_s *transport);
};

typedef struct kb_message_writer_s kb_message_writer_t;

// typedef message_error_t kb_writer_error_t;

// inline kb_writer_error_t writer_error(kb_message_writer_t* writer);

/**
 * @brief Gets the current size of the written message
 * @param writer The message writer
 * @return The number of bytes written
 */
size_t writer_message_size(kb_message_writer_t* writer);

/**
 * @brief Sends the written message using the writer's send callback
 * @param writer The message writer
 * @return 0 on success, non-zero on failure
 */
int message_send(kb_message_writer_t *writer);

/**
 * @brief Writes a nil/NULL value to the writer
 * @param writer The message writer
 */
void message_write_nil(kb_message_writer_t* writer);

/**
 * @brief Writes a boolean value to the writer
 * @param writer The message writer
 * @param value The boolean value to write
 */
void message_write_bool(kb_message_writer_t* writer, bool value);

void message_write_i8(kb_message_writer_t *writer, int8_t value);
void message_write_i16(kb_message_writer_t *writer, int16_t value);
void message_write_i32(kb_message_writer_t *writer, int32_t value);
void message_write_i64(kb_message_writer_t *writer, int64_t value);
void message_write_int(kb_message_writer_t *writer, int64_t value);
void message_write_u8(kb_message_writer_t *writer, uint8_t value);
void message_write_u16(kb_message_writer_t *writer, uint16_t value);
void message_write_u32(kb_message_writer_t *writer, uint32_t value);
void message_write_u64(kb_message_writer_t *writer, uint64_t value);
void message_write_uint(kb_message_writer_t *writer, uint64_t value);
void message_write_float(kb_message_writer_t *writer, float value);

/**
 * @brief Writes a double-precision floating point value to the writer
 * @param writer The message writer
 * @param value The double value to write
 */
void message_write_double(kb_message_writer_t* writer, double value);

/**
 * @brief Writes a null-terminated string to the writer
 * @param writer The message writer
 * @param str The null-terminated string to write
 */
void message_write_cstr(kb_message_writer_t* writer, const char* str);

/**
 * @brief Writes a null-terminated string to the writer, or nil if the string is NULL
 * @param writer The message writer
 * @param str The null-terminated string to write, or NULL to write nil
 */
void message_write_cstr_or_nil(kb_message_writer_t* writer, const char* str);

/**
 * @brief Writes a string with explicit length to the writer
 * @param writer The message writer
 * @param str The string data to write
 * @param length The number of bytes to write
 */
void message_write_str(kb_message_writer_t* writer, const char* str, uint32_t length);

/**
 * @brief Writes a UTF-8 string with explicit length to the writer
 * @param writer The message writer
 * @param str The UTF-8 string data to write
 * @param length The number of bytes to write
 */
void message_write_utf8(kb_message_writer_t* writer, const char* str, uint32_t length);

/**
 * @brief Writes a null-terminated UTF-8 string to the writer
 * @param writer The message writer
 * @param str The null-terminated UTF-8 string to write
 */
void message_write_utf8_cstr(kb_message_writer_t* writer, const char* str);

/**
 * @brief Writes binary data to the writer
 * @param writer The message writer
 * @param data The binary data to write
 * @param size The number of bytes to write
 */
void message_write_bin(kb_message_writer_t* writer, const char* data, uint32_t size);

/**
 * Opens a map.
 *
 * `count * 2` elements must follow, and message_finish_map() must be called
 * when done.
 *
 * If you do not know the number of elements to be written ahead of time, call
 * message_build_map() instead.
 *
 * Remember that while map elements in MessagePack are implicitly ordered,
 * they are not ordered in JSON. If you need elements to be read back
 * in the order they are written, consider use an array instead.
 *
 * @see message_complete_map()
 * @see message_build_map() to count the number of key/value pairs automatically
 */
void message_start_map(kb_message_writer_t* writer, uint32_t size);

/**
 * Finishes writing a map.
 *
 * This should be called only after a corresponding call to message_start_map()
 * and after the map contents are written.
 *
 * In debug mode (or if MPACK_WRITE_TRACKING is not 0), this will track writes
 * to ensure that the correct number of elements are written.
 *
 * @see message_start_map()
 */
void message_finish_map(kb_message_writer_t *writer);

/**
 * Starts building a map.
 *
 * An even number of elements must follow, and message_complete_map() must be
 * called when done. The number of elements is determined automatically.
 *
 * If you know ahead of time the number of elements in the map, it is more
 * efficient to call message_start_map() instead, even if you are already within
 * another open build.
 *
 * Builder containers can be nested within normal (known size) containers and
 * vice versa. You can call message_build_map(), then message_start_map() inside
 * it, then message_build_map() inside that, and so forth.
 *
 * A writer in build mode diverts writes to a builder buffer that allocates as
 * needed. Once the last map or array being built is completed, the deferred
 * message is composed with computed array and map sizes into the writer.
 * Builder maps and arrays are encoded exactly the same as ordinary maps and
 * arrays in the final message.
 *
 * This indirect encoding is costly, as it incurs at least an extra copy of all
 * data written within a builder (but not additional copies for nested
 * builders.) Expect a speed penalty of half or more.
 *
 * A good strategy is to use this during early development when your messages
 * are constantly changing, and then closer to release when your message
 * formats have stabilized, replace all your build calls with start calls with
 * pre-computed sizes. Or don't, if you find the builder has little impact on
 * performance, because even with builders MPack is extremely fast.
 *
 * @note When an array or map starts being built, nothing will be flushed
 *       until it is completed. If you are building a large message that
 *       does not fit in the output stream, you won't get an error about it
 *       until everything is written.
 *
 * @see message_complete_map() to complete this map
 * @see message_start_map() if you already know the size of the map
 */
void message_build_map(kb_message_writer_t* writer);

/**
 * Finishes writing a map.
 *
 * This should be called only after a corresponding call to message_start_map()
 * and after the map contents are written.
 *
 * In debug mode (or if MPACK_WRITE_TRACKING is not 0), this will track writes
 * to ensure that the correct number of elements are written.
 *
 * @see message_start_map()
 */
void message_complete_map(kb_message_writer_t* writer);

/**
 * Opens an array.
 *
 * `count` elements must follow, and message_finish_array() must be called
 * when done.
 *
 * If you do not know the number of elements to be written ahead of time, call
 * message_build_array() instead.
 *
 * @see message_finish_array()
 * @see message_build_array() to count the number of elements automatically
 */
void message_start_array(kb_message_writer_t* writer, uint32_t size);

/**
 * Finishes writing an array.
 *
 * This should be called only after a corresponding call to mpack_start_array()
 * and after the array contents are written.
 *
 * In debug mode (or if MPACK_WRITE_TRACKING is not 0), this will track writes
 * to ensure that the correct number of elements are written.
 *
 * @see message_start_array()
 */
void message_finish_array(kb_message_writer_t *writer);

/**
 * Starts building an array.
 *
 * Elements must follow, and message_complete_array() must be called when done. The
 * number of elements is determined automatically.
 *
 * If you know ahead of time the number of elements in the array, it is more
 * efficient to call message_start_array() instead, even if you are already
 * within another open build.
 *
 * Builder containers can be nested within normal (known size) containers and
 * vice versa. You can call message_build_array(), then message_start_array()
 * inside it, then message_build_array() inside that, and so forth.
 *
 * @see message_complete_array() to complete this array
 * @see message_start_array() if you already know the size of the array
 * @see message_build_map() for implementation details
 */
void message_build_array(kb_message_writer_t *writer);

/**
 * Finishes writing an array.
 *
 * This should be called only after a corresponding call to message_start_array()
 * and after the array contents are written.
 *
 * In debug mode (or if MPACK_WRITE_TRACKING is not 0), this will track writes
 * to ensure that the correct number of elements are written.
 *
 * @see message_start_array()
 */
void message_complete_array(kb_message_writer_t* writer);

#if !defined(__cplusplus)
#define message_write(writer, value)       \
    _Generic(((void)0, value),             \
        int8_t: message_write_i8,          \
        int16_t: message_write_i16,        \
        int32_t: message_write_i32,        \
        int64_t: message_write_i64,        \
        uint8_t: message_write_u8,         \
        uint16_t: message_write_u16,       \
        uint32_t: message_write_u32,       \
        uint64_t: message_write_u64,       \
        bool: message_write_bool,          \
        float: message_write_float,        \
        double: message_write_double,      \
        char *: message_write_cstr_or_nil, \
        const char *: message_write_cstr_or_nil)(writer, value)

/**
 * @def message_write_kv(writer, key, value)
 *
 * Type-generic writer for key-value pairs of null-terminated string
 * keys and primitive values.
 *
 * @warning @a writer may be evaluated multiple times.
 *
 * @warning In C11, the indentifiers `true`, `false` and `NULL` are
 * all of type `int`, not `bool` or `void*`! They will emit unexpected
 * types when passed uncast, so be careful when using them.
 *
 * @param writer The writer.
 * @param key A null-terminated C string.
 * @param value A primitive type supported by message_write().
 */
#define message_write_kv(writer, key, value) do {     \
    message_write_cstr(writer, key);                  \
    message_write(writer, value);                     \
} while (0)
#endif