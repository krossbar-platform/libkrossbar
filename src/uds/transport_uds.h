#pragma once

#include "event_manager_uds.h"
#include "../transport.h"

struct io_uring;

#pragma pack(push, 1)
struct message_header_s
{
    uint8_t magic;
    uint32_t data_len;
};
#pragma pack(pop)

typedef struct message_header_s message_header_t;

struct message_buffer_s
{
    char *data;
    size_t data_size;
    size_t current_offset;
    bool header_sent;
};

typedef struct message_buffer_s message_buffer_t;

struct out_messages_s
{
    message_buffer_t message;
    struct out_messages_s *prev; /* needed for a doubly-linked list only */
    struct out_messages_s *next; /* needed for singly- or doubly-linked lists */
};

typedef struct out_messages_s out_messages_t;

struct kb_transport_uds_s
{
    kb_transport_t base;
    int sock_fd;
    size_t max_message_size;
    // Out messages for writing into the socket
    out_messages_t *out_messages;
    // Current number of buffered messages
    size_t out_message_count;
    // Max number of buffered messages
    size_t max_buffered_messages;
    // Incoming message buffer
    message_buffer_t in_message;
};

typedef struct kb_transport_uds_s kb_transport_uds_t;

kb_transport_t *transport_uds_init(const char *name, int fd, size_t max_message_size, size_t max_buffered_messages, struct io_uring *ring, log4c_category_t *logger);

kb_message_writer_t *transport_uds_message_init(kb_transport_t *transport);
int transport_uds_message_send(kb_transport_t *transport, kb_message_writer_t *writer);
int transport_uds_write_messages(kb_transport_t *transport);

kb_message_t *transport_uds_message_receive(kb_transport_t *transport);
int transport_uds_message_release(kb_transport_t *transport, kb_message_t *message);

void transport_uds_destroy(kb_transport_t *transport);