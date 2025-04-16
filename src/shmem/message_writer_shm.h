#pragma once

#include "message_writer.h"
#include "transport_shm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Shared memory implementation of message writer
 */
struct kb_message_writer_shm_s
{
    kb_message_writer_t base;      // Base message writer interface
    kb_message_header_t *header;   // Header for the message being written
    kb_transport_shm_t *transport; // Transport used for sending the message
};

typedef struct kb_message_writer_shm_s kb_message_writer_shm_t;

/**
 * @brief Initialize a shared memory message writer
 *
 * @param transport Transport to use for sending the message
 * @param header Message header for the message being written
 * @param buffer Buffer for the message payload
 * @return Initialized message writer or NULL on failure
 */
kb_message_writer_shm_t *message_writer_shm_init(kb_transport_shm_t *transport,
                                                 kb_message_header_t *header,
                                                 char *buffer);

/**
 * @brief Send a message through the shared memory transport
 *
 * @param writer Message writer containing the message to send
 * @return 0 on success, negative error code on failure
 */
int message_writer_shm_send(kb_message_writer_t *writer);

/**
 * @brief Cancel a message being written and release resources
 *
 * @param writer Message writer to cancel
 */
void message_writer_shm_cancel(kb_message_writer_t *writer);

#ifdef __cplusplus
} // extern "C"
#endif
