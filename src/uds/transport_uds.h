#pragma once

#include "transport.h"

struct io_uring;

struct message_buffer_s
{
    char *data;
    size_t data_size;
    size_t current_offset;
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
    struct io_uring *ring;
    const char *name;
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

kb_transport_t *transport_uds_init(const char *name, int fd, size_t max_message_size, size_t max_buffered_messages);

kb_message_writer_t *transport_uds_message_init(kb_transport_t *transport);
int transport_uds_message_send(kb_transport_t *transport, kb_message_writer_t *writer);

kb_message_t *transport_uds_message_receive(kb_transport_t *transport);
int transport_uds_message_release(kb_transport_t *transport, kb_message_t *message);

int transport_uds_get_fd(kb_transport_t *transport);
void transport_uds_destroy(kb_transport_t *transport);