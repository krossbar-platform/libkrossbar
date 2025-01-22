#pragma once

#include <stddef.h>
#include <stdbool.h>

#include <mpack.h>

#include "transport.h"

struct kb_message_s {
    mpack_reader_t reader;
    void (*destroy)(struct kb_message_s *message);
};

typedef struct kb_message_s kb_message_t;

typedef mpack_tag_t kb_message_tag_t;

/**
 * @brief Initialize a new message with data buffer
 * @param data Pointer to message data buffer
 * @param size Size of the data buffer
 * @return Pointer to initialized message or NULL on error
 */
kb_message_t *message_init(void *data, size_t size);

/**
 * @brief Destroy a message and free resources
 * @param message Message to destroy
 */
void message_destroy(kb_message_t *message);

kb_message_tag_t message_read_tag(kb_message_t *message);
kb_message_tag_t message_peek_tag(kb_message_t *message);
