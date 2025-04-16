#pragma once

#include <stddef.h>
#include <stdbool.h>

#define MPACK_FLOAT 1
#define MPACK_DOUBLE 1
#include <mpack-common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef mpack_tag_t kb_message_tag_t;

struct mpack_reader_t;

struct kb_message_s
{
    uint64_t id;
    struct mpack_reader_t *data_reader;
    int (*destroy)(struct kb_message_s *message);
};

typedef struct kb_message_s kb_message_t;

/**
 * @brief Initialize a new message with data buffer
 * @param message Pointer to message to initialize
 * @param data Pointer to message data buffer
 * @param size Size of the data buffer
 * @return Pointer to initialized message or NULL on error
 */
void message_init(kb_message_t *mesage, void *data, size_t size);

/**
 * @brief Destroy a message and free resources
 * @param message Message to destroy
 */
void message_destroy(kb_message_t *message);

kb_message_tag_t message_read_tag(kb_message_t *message);
kb_message_tag_t message_peek_tag(kb_message_t *message);

/**
 * Reads bytes from a string, binary blob or extension object in-place in
 * the buffer. This can be used to avoid copying the data.
 *
 * If the bytes are from a string, the string is not null-terminated! Use
 * mpack_read_cstr() to copy the string into a buffer and add a null-terminator.
 *
 * The returned pointer is invalidated on the next read, or when the buffer
 * is destroyed.
 *
 * The reader will move data around in the buffer if needed to ensure that
 * the pointer can always be returned, so this should only be used if
 * count is very small compared to the buffer size. If you need to check
 * whether a small size is reasonable (for example you intend to handle small and
 * large sizes differently), you can call mpack_should_read_bytes_inplace().
 *
 * This can be called multiple times for a single str, bin or ext
 * to read the data in chunks. The total data read must add up
 * to the size of the object.
 *
 * NULL is returned if the reader is in an error state.
 *
 * @throws mpack_error_too_big if the requested size is larger than the buffer size
 */
const char *message_read_bytes(kb_message_t *message, size_t count);

#ifdef __cplusplus
} // extern "C"
#endif
