#pragma once

#include <stddef.h>
#include <stdbool.h>

#include <bson.h>

#ifdef __cplusplus
extern "C" {
#endif

struct kb_message_s
{
    uint64_t id;
    bson_t *document;
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
void message_init(kb_message_t *mesage, uint8_t *data, size_t size);

/**
 * @brief Destroy a message and free resources
 * @param message Message to destroy
 */
void message_destroy(kb_message_t *message);

/**
 * @brief Get the document from the message
 *
 * @param message A message
 * @return A pointer to the document in the message
 */
bson_t *message_get_document(kb_message_t *message);

#ifdef __cplusplus
} // extern "C"
#endif
