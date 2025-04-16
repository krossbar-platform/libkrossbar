#pragma once

#include <uthash.h>
#include <stdatomic.h>

#include "transport.h"
#include "message.h"

/**
 * @brief Message type enumeration for RPC communication
 */
enum kb_message_type_e
{
    KB_MESSAGE_TYPE_MESSAGE = 0,      // One-way message
    KB_MESSAGE_TYPE_CALL = 1,         // Call requesting a response
    KB_MESSAGE_TYPE_SUBSCRIPTION = 2, // Subscription request
    KB_MESSAGE_TYPE_RESPONSE = 3,     // Response to a call
};

typedef enum kb_message_type_e kb_message_type_t;

/**
 * @brief Call registry entry for tracking outgoing calls
 */
struct kb_call_entry_s
{
    int id;                                   // Call ID
    kb_message_type_t type;                   // Message type
    void (*callback)(kb_message_t *, void *); // Callback function
    void *context;                            // User context for callback
    UT_hash_handle hh;                        // Hash handle for uthash
};

typedef struct kb_call_entry_s kb_call_entry_t;

/**
 * @brief Registry for tracking outgoing calls
 */
struct kb_call_registry_s
{
    kb_call_entry_t *entries; // Hash table of call entries
};

typedef struct kb_call_registry_s kb_call_registry_t;

/**
 * @brief RPC module for handling remote procedure calls
 */
struct kb_rpc_s
{
    kb_call_registry_t calls_registry; // Registry for tracking outgoing calls
    kb_transport_t *transport;         // Transport for sending/receiving messages
    log4c_category_t *logger;          // Logger for debugging
    atomic_uint_fast64_t id_counter;   // Counter for generating unique message IDs
};

typedef struct kb_rpc_s kb_rpc_t;

/**
 * @brief RPC message writer for constructing outgoing messages
 */
struct kb_rpc_message_writer_s
{
    kb_message_writer_t base;                 // Base message writer interface
    kb_message_writer_t *transport_writer;    // Underlying transport writer
    kb_rpc_t *rpc;                            // RPC module
    uint64_t id;                              // Message ID
    kb_message_type_t type;                   // Message type
    void (*callback)(kb_message_t *, void *); // Callback function for responses
    void *context;                            // User context for callback
};

typedef struct kb_rpc_message_writer_s kb_rpc_message_writer_t;

/**
 * @brief RPC message wrapper for incoming messages
 */
struct kb_rpc_message_s
{
    kb_message_t *message;  // Underlying transport message
    kb_rpc_t *rpc;          // RPC module
    uint64_t id;            // Message ID
    kb_message_type_t type; // Message type
};

typedef struct kb_rpc_message_s kb_rpc_message_t;

/**
 * @brief Generate a new unique message ID
 *
 * @param rpc RPC module
 * @return New unique message ID
 */
uint64_t next_id(kb_rpc_t *rpc);

/**
 * @brief Initialize a new RPC module
 *
 * @param transport Transport for sending/receiving messages
 * @param logger Logger for debugging
 * @return Initialized RPC module or NULL on failure
 */
kb_rpc_t *rpc_init(kb_transport_t *transport, log4c_category_t *logger);

/**
 * @brief Destroy an RPC module and free all resources
 *
 * @param rpc RPC module to destroy
 */
void rpc_destroy(kb_rpc_t *rpc);

/**
 * @brief Create a new one-way message
 *
 * @param rpc RPC module
 * @return Message writer for constructing the message
 */
kb_message_writer_t *rpc_message(kb_rpc_t *rpc);

/**
 * @brief Create a new RPC call expecting a response
 *
 * @param rpc RPC module
 * @param callback Callback function to call when response is received
 * @param context User context for callback
 * @return Message writer for constructing the call
 */
kb_message_writer_t *rpc_call(kb_rpc_t *rpc, void (*callback)(kb_message_t *, void *), void *context);

/**
 * @brief Create a new subscription request
 *
 * @param rpc RPC module
 * @param callback Callback function to call for each notification
 * @param context User context for callback
 * @return Message writer for constructing the subscription
 */
kb_message_writer_t *rpc_subscribe(kb_rpc_t *rpc, void (*callback)(kb_message_t *, void *), void *context);

/**
 * @brief Wrap a transport message with RPC information
 *
 * @param rpc RPC module
 * @param writer Transport message writer
 * @param type Message type
 * @param callback Callback function for responses
 * @param context User context for callback
 * @return RPC message writer
 */
kb_message_writer_t *rpc_wrap_transport_message(kb_rpc_t *rpc, kb_message_writer_t *writer,
                                                kb_message_type_t type, void (*callback)(kb_message_t *, void *),
                                                void *context);

/**
 * @brief Send an RPC message
 *
 * @param writer Message writer containing the message to send
 * @return 0 on success, negative error code on failure
 */
int rpc_message_send(kb_message_writer_t *writer);

/**
 * @brief Cancel an RPC message and free resources
 *
 * @param writer Message writer to cancel
 */
void rpc_message_cancel(kb_message_writer_t *writer);

/**
 * @brief Get the message body from an RPC message
 *
 * @param message RPC message
 * @return Underlying transport message
 */
kb_message_t *rpc_message_body(kb_rpc_message_t *message);

/**
 * @brief Create a response to an RPC call
 *
 * @param message Original RPC call message
 * @return Message writer for constructing the response
 */
kb_message_writer_t *rpc_message_respond(kb_rpc_message_t *message);

/**
 * @brief Release an RPC message and free resources
 *
 * @param message RPC message to release
 */
void rpc_message_release(kb_rpc_message_t *message);

/**
 * @brief Handle an incoming transport message
 *
 * @param rpc RPC module
 * @param message Incoming transport message
 * @return RPC message wrapper or NULL on error
 */
kb_rpc_message_t *rpc_handle_incoming_message(kb_rpc_t *rpc, kb_message_t *message);

/**
 * @brief Write the RPC header to a message
 *
 * @param message Message writer
 * @param id Message ID
 * @param type Message type
 */
void rpc_write_message_header(kb_message_writer_t *message, uint64_t id, kb_message_type_t type);
