#pragma once

#include "message_writer.h"
#include "transport_uds.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unix Domain Socket implementation of message writer
 */
struct kb_message_writer_uds_s
{
    kb_message_writer_t base;      // Base message writer interface
    kb_transport_uds_t *transport; // Transport used for sending the message
};

typedef struct kb_message_writer_uds_s kb_message_writer_uds_t;

/**
 * @brief Initialize a UDS message writer
 *
 * @param transport Transport to use for sending the message
 * @param buffer Buffer for the message payload
 * @param buffer_size Size of the message buffer
 * @return Initialized message writer or NULL on failure
 */
kb_message_writer_uds_t *message_writer_uds_init(kb_transport_uds_t *transport,
                                                 char *buffer,
                                                 size_t buffer_size);

/**
 * @brief Send a message through the UDS transport
 *
 * @param writer Message writer containing the message to send
 * @return 0 on success, negative error code on failure
 */
int message_writer_uds_send(kb_message_writer_t *writer);

/**
 * @brief Cancel a message being written and release resources
 *
 * @param writer Message writer to cancel
 */
void message_writer_uds_cancel(kb_message_writer_t *writer);

#ifdef __cplusplus
} // extern "C"
#endif
