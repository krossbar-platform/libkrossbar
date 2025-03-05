#pragma once

#include <semaphore.h>
#include <stdatomic.h>

#include "transport.h"
#include "event_manager_shm.h"

struct kb_message_writer_shm_s;

struct kb_arena_header_s
{
    sem_t write_sem;
    sem_t read_sem;
    size_t write_offset;
    size_t read_offset;
    size_t size;
    uint32_t num_messages;
};

typedef struct kb_arena_header_s kb_arena_header_t;

struct kb_arena_s
{
    kb_arena_header_t *header;
    void *addr;
    int shm_fd;
};

typedef struct kb_arena_s kb_arena_t;

struct kb_message_header_s {
    size_t next_message_offset;
    size_t size;
};

typedef struct kb_message_header_s kb_message_header_t;

struct kb_transport_shm_s
{
    kb_transport_t base;
    kb_event_manager_shm_t event_manager;
    kb_arena_t read_arena;
    kb_arena_t write_arena;
    size_t max_message_size;
    kb_message_header_t *last_message;
};

typedef struct kb_transport_shm_s kb_transport_shm_t;

int transport_shm_create_mapping(const char *name, size_t buffer_size, log4c_category_t *logger);
size_t transport_shm_get_mapping_size(int map_fd);

kb_transport_t *transport_shm_init(const char *name, int read_fd, int write_fd,
                                   size_t max_message_size, struct io_uring *ring, log4c_category_t *logger);

kb_message_writer_t* transport_shm_message_init(kb_transport_t *transport);
int transport_shm_message_send(kb_transport_t *transport, kb_message_writer_t *writer);

kb_message_t *transport_shm_message_receive(kb_transport_t *transport);
int transport_shm_message_release(kb_transport_t *transport, kb_message_t *message);

void transport_shm_destroy(kb_transport_t *transport);
