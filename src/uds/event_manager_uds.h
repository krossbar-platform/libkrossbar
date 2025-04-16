#pragma once

#include "message.h"
#include "../event_manager.h"

struct kb_transport_uds_s;
struct io_uring;
struct io_uring_cqe;

/**
 * @brief Unix Domain Socket implementation of the event manager
 */
struct kb_event_manager_uds_s
{
    kb_event_manager_t base; // Base event manager interface

    kb_event_t read_event;  // Event triggered when data is available to read
    kb_event_t write_event; // Event triggered when buffer is available to write
};

typedef struct kb_event_manager_uds_s kb_event_manager_uds_t;

/**
 * @brief Create a UDS event manager
 *
 * @param transport Transport used for message passing
 * @param ring IO_URING instance for asynchronous operations
 * @param logger Logger for debugging
 * @return Initialized event manager or NULL on failure
 */
kb_event_manager_uds_t *event_manager_uds_create(struct kb_transport_uds_s *transport, struct io_uring *ring, log4c_category_t *logger);

/**
 * @brief Destroy a UDS event manager and release all resources
 *
 * @param manager Event manager to destroy
 */
void event_manager_uds_destroy(kb_event_manager_uds_t *manager);

/**
 * @brief Wait for socket to become readable
 *
 * @param manager Event manager to wait on
 */
void event_manager_uds_wait_readable(kb_event_manager_uds_t *manager);

/**
 * @brief Wait for socket to become writeable
 *
 * @param manager Event manager to wait on
 */
void event_manager_uds_wait_writeable(kb_event_manager_uds_t *manager);

/**
 * @brief Handle a completion event and retrieve associated message
 *
 * @param cqe Completion queue event from io_uring
 * @return Message associated with the event or NULL if no message is available
 */
kb_message_t *event_manager_uds_handle_event(struct io_uring_cqe *cqe);
