#pragma once

#include <semaphore.h>
#include <stdatomic.h>

#include "../transport.h"
#include "event_manager_shm.h"
#include "allocator.h"

struct kb_message_writer_shm_s;

/**
 * @brief Arena header structure at the beginning of the shared memory region
 */
struct kb_arena_header_s
{
    size_t size;                 // Total size of the arena
    uint32_t num_messages;       // Number of messages in the arena
    uint32_t futex;              // Futex for synchronization
    size_t first_message_offset; // Offset of the first message in the arena
    size_t last_message_offset;  // Offset of the last message in the arena
};

typedef struct kb_arena_header_s kb_arena_header_t;

/**
 * @brief Memory arena structure for message passing
 */
struct kb_arena_s
{
    kb_arena_header_t *header; // Header for the shared memory region
    kb_allocator_t *allocator; // Memory allocator for the arena
    int shm_fd;                // Shared memory file descriptor
};

typedef struct kb_arena_s kb_arena_t;

/**
 * @brief Message header structure preceding each message
 */
struct kb_message_header_s
{
    size_t size;                // Size of the message payload
    size_t next_message_offset; // Offset of the next message in the arena
};

typedef struct kb_message_header_s kb_message_header_t;

/**
 * @brief Shared memory transport implementation
 */
struct kb_transport_shm_s
{
    kb_transport_t base;     // Base transport interface
    kb_arena_t read_arena;   // Arena for reading messages
    kb_arena_t write_arena;  // Arena for writing messages
    size_t max_message_size; // Maximum message size for this transport
};

typedef struct kb_transport_shm_s kb_transport_shm_t;

/**
 * @brief Create a new shared memory mapping
 *
 * @param name Name of the shared memory segment
 * @param buffer_size Size of the shared memory segment
 * @param logger Logger for debugging
 * @return File descriptor for the shared memory segment, or -1 on error
 */
int transport_shm_create_mapping(const char *name, size_t buffer_size, log4c_category_t *logger);

/**
 * @brief Get the size of an existing shared memory mapping from the memory header
 *
 * @param map_fd File descriptor for the shared memory segment
 * @param logger Logger for debugging
 * @return Size of the shared memory segment, or 0 on error
 */
size_t transport_shm_get_mapping_size(int map_fd, log4c_category_t *logger);

/**
 * @brief Initialize a shared memory transport
 *
 * @param name Name of the transport (for debugging)
 * @param read_fd File descriptor for the read arena
 * @param write_fd File descriptor for the write arena
 * @param max_message_size Maximum size of messages for this transport
 * @param ring IO_URING instance for asynchronous operations
 * @param logger Logger for debugging
 * @return Initialized transport or NULL on failure
 */
kb_transport_t *transport_shm_init(const char *name, int read_fd, int write_fd,
                                   size_t max_message_size, struct io_uring *ring, log4c_category_t *logger);

/**
 * @brief Get a message header from its offset in the arena
 *
 * @param arena Arena containing the message
 * @param offset Offset of the message in the arena
 * @return Pointer to the message header
 */
kb_message_header_t *transport_message_from_offset(kb_arena_t *arena, size_t offset);

/**
 * @brief Get the offset of a message in the arena
 *
 * @param arena Arena containing the message
 * @param message_header Pointer to the message header
 * @return Offset of the message in the arena
 */
size_t transport_message_offset(kb_arena_t *arena, kb_message_header_t *message_header);

/**
 * @brief Initialize a new message for writing
 *
 * @param transport Transport to use for sending the message
 * @return Message writer or NULL on failure
 */
kb_message_writer_t* transport_shm_message_init(kb_transport_t *transport);

/**
 * @brief Send a message through the transport
 *
 * @param transport Transport to use for sending the message
 * @param writer Message writer containing the message to send
 * @return 0 on success, negative error code on failure
 */
int transport_shm_message_send(kb_transport_t *transport, kb_message_writer_t *writer);

/**
 * @brief Receive a message from the transport
 *
 * @param transport Transport to receive the message from
 * @return Received message or NULL if no message is available
 */
kb_message_t *transport_shm_message_receive(kb_transport_t *transport);

/**
 * @brief Release a received message back to the transport
 *
 * @param transport Transport that provided the message
 * @param message Message to release
 * @return 0 on success, negative error code on failure
 */
int transport_shm_message_release(kb_transport_t *transport, kb_message_t *message);

/**
 * @brief Destroy a shared memory transport and release all resources
 *
 * @param transport Transport to destroy
 */
void transport_shm_destroy(kb_transport_t *transport);
