#pragma once

#include "event_manager_uds.h"
#include "../transport.h"

#ifdef __cplusplus
extern "C" {
#endif

struct io_uring;

/**
 * @brief Message header structure for UDS transport
 */
#pragma pack(push, 1)
struct message_header_s
{
    uint8_t magic;     // Magic number for validation
    uint32_t data_len; // Length of the message data
};
#pragma pack(pop)

typedef struct message_header_s message_header_t;

/**
 * @brief Buffer for message data
 */
struct message_buffer_s
{
    char *data;            // Buffer data
    size_t data_size;      // Size of the buffer
    size_t current_offset; // Current read/write position
    bool header_sent;      // Whether the header has been sent
};

typedef struct message_buffer_s message_buffer_t;

/**
 * @brief Queue element for outgoing messages
 */
struct out_messages_s
{
    message_buffer_t message;    // Message buffer
    struct out_messages_s *prev; // Previous message in the queue
    struct out_messages_s *next; // Next message in the queue
};

typedef struct out_messages_s out_messages_t;

/**
 * @brief Unix Domain Socket transport implementation
 */
struct kb_transport_uds_s
{
    kb_transport_t base;          // Base transport interface
    int sock_fd;                  // Socket file descriptor
    size_t max_message_size;      // Maximum message size
    out_messages_t *out_messages; // Queue of outgoing messages
    size_t out_message_count;     // Current number of buffered messages
    size_t max_buffered_messages; // Maximum number of buffered messages
    message_buffer_t in_message;  // Buffer for incoming message
};

typedef struct kb_transport_uds_s kb_transport_uds_t;

/**
 * @brief Initialize a UDS transport
 *
 * @param name Name of the transport (for debugging)
 * @param fd Socket file descriptor
 * @param max_message_size Maximum size of messages for this transport
 * @param max_buffered_messages Maximum number of messages to buffer
 * @param ring IO_URING instance for asynchronous operations
 * @param logger Logger for debugging
 * @return Initialized transport or NULL on failure
 */
kb_transport_t *transport_uds_init(const char *name, int fd, size_t max_message_size, size_t max_buffered_messages, struct io_uring *ring, log4c_category_t *logger);

/**
 * @brief Initialize a new message for writing
 *
 * @param transport Transport to use for sending the message
 * @return Initialized message writer or NULL on failure
 */
kb_message_writer_t *transport_uds_message_init(kb_transport_t *transport);

/**
 * @brief Send a message through the transport
 *
 * @param transport Transport to use for sending the message
 * @param writer Message writer containing the message to send
 * @return 0 on success, negative error code on failure
 */
int transport_uds_message_send(kb_transport_t *transport, kb_message_writer_t *writer);

/**
 * @brief Write buffered messages to the socket
 *
 * @param transport Transport to write messages from
 * @return 0 on success, negative error code on failure
 */
int transport_uds_write_messages(kb_transport_t *transport);

/**
 * @brief Receive a message from the transport
 *
 * @param transport Transport to receive the message from
 * @return Received message or NULL if no message is available
 */
kb_message_t *transport_uds_message_receive(kb_transport_t *transport);

/**
 * @brief Release a received message back to the transport
 *
 * @param transport Transport that provided the message
 * @param message Message to release
 * @return 0 on success, negative error code on failure
 */
int transport_uds_message_release(kb_transport_t *transport, kb_message_t *message);

/**
 * @brief Destroy a UDS transport and release all resources
 *
 * @param transport Transport to destroy
 */
void transport_uds_destroy(kb_transport_t *transport);

#ifdef __cplusplus
} // extern "C"
#endif
