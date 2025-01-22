#pragma once

#include <semaphore.h>

#include "transport.h"

struct kb_message_writer_shm_s;

struct kb_arena_header_s
{
    sem_t write_sem;
    sem_t read_sem;
    uint32_t write_offset;
    uint32_t read_offset;
};

typedef struct kb_arena_header_s kb_arena_header_t;

struct kb_arena_s
{
    kb_arena_header_t *header;
    void *addr;
    size_t size;
};

typedef struct kb_arena_s kb_arena_t;

struct kb_message_header_s {
    struct kb_message_header_s* next_message;
    size_t size;
};

typedef struct kb_message_header_s kb_message_header_t;

struct kb_transport_shm_s
{
    kb_transport_t base;
    kb_arena_t arena;
    const char *name;
    int shm_fd;
    size_t message_size;
};

typedef struct kb_transport_shm_s kb_transport_shm_t;

kb_transport_t* transport_shm_init(const char *name, size_t buffer_size, size_t message_size);

kb_message_writer_t* transport_shm_message_init(kb_transport_t *transport);
int transport_shm_message_send(kb_transport_t *transport, kb_message_writer_t *writer);

kb_message_t *transport_shm_message_receive(kb_transport_t *transport);
int transport_shm_message_release(kb_transport_t *transport, kb_message_t *writer);

int transport_shm_get_fd(kb_transport_t *transport);
void transport_shm_destroy(kb_transport_t *transport);
