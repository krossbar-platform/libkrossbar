#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <log4c/category.h>
#include <bson.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct kb_message_writer_s
{
    bson_t *data_writer;
    uint8_t *buffer;
    log4c_category_t *logger;
    int (*send)(struct kb_message_writer_s *writer);
    void (*cancel)(struct kb_message_writer_s *writer);
};

typedef struct kb_message_writer_s kb_message_writer_t;

// typedef message_error_t kb_writer_error_t;

// inline kb_writer_error_t writer_error(kb_message_writer_t* writer);

void message_writer_init(kb_message_writer_t *writer, uint8_t *data, size_t size, log4c_category_t *logger);

/**
 * @brief Gets the document from the message writer
 * @param writer The message writer
 * @return A pointer to the document in the message writer
 */
bson_t *message_writer_get_document(kb_message_writer_t *writer);

/**
 * @brief Gets the current size of the written message
 * @param writer The message writer
 * @return The number of bytes written
 */
size_t message_writer_size(kb_message_writer_t *writer);

/**
 * @brief Sends the written message using the writer's send callback
 * @param writer The message writer
 * @return 0 on success, non-zero on failure
 */
int message_send(kb_message_writer_t *writer);

/**
 * @brief Cancels sending the message
 * @param writer The message writer
 */
void message_cancel(kb_message_writer_t *writer);

#ifdef __cplusplus
} // extern "C"
#endif