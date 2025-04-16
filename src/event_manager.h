#pragma once

#include <log4c/category.h>

#include "message.h"

struct io_uring_cqe;
struct io_uring;

/**
 * @brief Event manager interface for handling asynchronous I/O
 */
struct kb_event_manager_s
{
    log4c_category_t *logger;         // Logger for debugging
    struct kb_transport_s *transport; // Transport for message passing
    struct io_uring *ring;            // IO_URING instance for async operations

    /**
     * @brief Handle a completion event and retrieve associated message
     * @param cqe Completion queue event from io_uring
     * @return Message associated with the event or NULL if no message is available
     */
    kb_message_t *(*handle_event)(struct io_uring_cqe *cqe);
};

typedef struct kb_event_manager_s kb_event_manager_t;

/**
 * @brief Event types for transport operations
 */
enum kb_event_type_u
{
    KB_UDS_EVENT_READABLE,  // Data is available to read
    KB_UDS_EVENT_WRITEABLE, // Buffer is available to write
    KB_UDS_EVENT_MAX        // Maximum event type value
};

typedef enum kb_event_type_u kb_event_type_t;

/**
 * @brief Event structure for asynchronous I/O
 */
struct kb_event_s
{
    kb_event_manager_t *manager; // Event manager that owns this event
    kb_event_type_t event_type;  // Type of this event
};

typedef struct kb_event_s kb_event_t;

/**
 * @brief Handle a completion event and retrieve associated message
 *
 * @param manager Event manager to handle the event
 * @param cqe Completion queue event from io_uring
 * @return Message associated with the event or NULL if no message is available
 */
inline kb_message_t *event_manager_handle_event(kb_event_manager_t *manager, struct io_uring_cqe *cqe)
{
    return manager->handle_event(cqe);
}