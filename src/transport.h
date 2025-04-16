#pragma once

#include <log4c/category.h>

#include "event_manager.h"
#include "message_writer.h"
#include "message.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transport interface for message passing
 */
struct kb_transport_s
{
    const char *name;                  // Name of the transport (for debugging)
    log4c_category_t *logger;          // Logger for debugging
    kb_event_manager_t *event_manager; // Event manager for asynchronous operations

    /**
     * @brief Initialize a new message for writing
     * @param transport Transport to use for sending the message
     * @return Initialized message writer or NULL on failure
     */
    kb_message_writer_t *(*message_init)(struct kb_transport_s *transport);

    /**
     * @brief Receive a message from the transport
     * @param transport Transport to receive the message from
     * @return Received message or NULL if no message is available
     */
    kb_message_t *(*message_receive)(struct kb_transport_s *transport);

    /**
     * @brief Destroy the transport and release all resources
     * @param transport Transport to destroy
     */
    void (*destroy)(struct kb_transport_s *transport);
};

typedef struct kb_transport_s kb_transport_t;

/**
 * @brief Initialize a new message for writing
 *
 * @param transport Transport to use for sending the message
 * @return Initialized message writer or NULL on failure
 */
inline kb_message_writer_t *transport_message_init(kb_transport_t *transport)
{
    return transport->message_init(transport);
}

/**
 * @brief Get the event manager for the transport
 *
 * @param transport Transport to get the event manager from
 * @return Event manager for the transport
 */
inline kb_event_manager_t *transport_get_event_manager(kb_transport_t *transport)
{
    return transport->event_manager;
}

/**
 * @brief Receive a message from the transport
 *
 * @param transport Transport to receive the message from
 * @return Received message or NULL if no message is available
 */
inline kb_message_t *transport_message_receive(kb_transport_t *transport)
{
    return transport->message_receive(transport);
}

/**
 * @brief Destroy the transport and release all resources
 *
 * @param transport Transport to destroy
 */
inline void transport_destroy(kb_transport_t *transport)
{
    transport->destroy(transport);
}

#ifdef __cplusplus
} // extern "C"
#endif
