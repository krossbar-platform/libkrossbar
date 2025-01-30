#pragma once

#include "transport.h"

struct io_uring;

struct kb_transport_uds_s
{
    kb_transport_t base;
    struct io_uring *ring;
    const char *name;
    int shm_fd;
    size_t max_message_size;
};

typedef struct kb_transport_uds_s kb_transport_uds_t;

kb_transport_t *transport_uds_init(const char *name, int fd, size_t max_message_size, struct io_uring *ring);

kb_message_writer_t *transport_uds_message_init(kb_transport_t *transport);
int transport_uds_message_send(kb_transport_t *transport, kb_message_writer_t *writer);

kb_message_t *transport_uds_message_receive(kb_transport_t *transport);
int transport_uds_message_release(kb_transport_t *transport, kb_message_t *message);

int transport_uds_get_fd(kb_transport_t *transport);
void transport_uds_destroy(kb_transport_t *transport);