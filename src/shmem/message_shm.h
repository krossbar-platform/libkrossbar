#pragma once

#include "message.h"
#include "transport_shm.h"

/**
 * @brief Shared memory implementation of message reader
 */
struct kb_message_shm_s
{
    kb_message_t base;             // Base message interface
    kb_message_header_t *header;   // Header for the message being read
    kb_transport_shm_t *transport; // Transport that provided the message
};

typedef struct kb_message_shm_s kb_message_shm_t;

/**
 * @brief Initialize a shared memory message reader
 *
 * @param transport Transport that provided the message
 * @param header Message header for the message being read
 * @param buffer Buffer containing the message payload
 * @return Initialized message reader or NULL on failure
 */
kb_message_shm_t *message_shm_init(kb_transport_shm_t *transport,
                                   kb_message_header_t *header,
                                   char *buffer);

/**
 * @brief Clean up resources used by a message
 *
 * @param message Message to clean up
 * @return 0 on success, negative error code on failure
 */
int message_shm_clean(kb_message_t *message);
