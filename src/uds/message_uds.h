#pragma once

#include "message.h"
#include "transport_uds.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unix Domain Socket implementation of message reader
 */
struct kb_message_uds_s
{
    kb_message_t base;             // Base message interface
    kb_transport_uds_t *transport; // Transport that provided the message
};

typedef struct kb_message_uds_s kb_message_uds_t;

/**
 * @brief Initialize a UDS message reader
 *
 * @param transport Transport that provided the message
 * @param buffer Buffer containing the message payload
 * @param size Size of the message buffer
 * @return Initialized message reader or NULL on failure
 */
kb_message_uds_t *message_uds_init(kb_transport_uds_t *transport,
                                   char *buffer,
                                   size_t size);

/**
 * @brief Clean up resources used by a message
 *
 * @param message Message to clean up
 * @return 0 on success, negative error code on failure
 */
int message_uds_clean(kb_message_t *message);

#ifdef __cplusplus
} // extern "C"
#endif
