#include "transport_shm.h"

#include <assert.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <liburing.h>

#include "message_writer_shm.h"
#include "message_shm.h"

#define RING_QUEUE_DEPTH 32

enum ring_op_e
{
    RING_OP_SEND,
    RING_OP_RECV
};

typedef enum ring_op_e ring_op_t;

kb_transport_t *transport_shm_init(const char *name, size_t buffer_size, size_t max_message_size, struct io_uring *ring, log4c_category_t *logger)
{
    assert(ring != NULL);

    if (buffer_size < max_message_size)
    {
        perror("Buffer size is smaller than message size");
        return NULL;
    }

    kb_transport_shm_t* transport = calloc(1, sizeof(kb_transport_shm_t));
    if (transport == NULL) {
        perror("calloc failed");
        return NULL;
    }

    transport->name = strdup(name);
    transport->max_message_size = max_message_size;

    transport->base.logger = logger;
    transport->base.message_init = transport_shm_message_init;
    transport->base.message_receive = transport_shm_message_receive;
    transport->base.get_fd = transport_shm_get_fd;
    transport->base.destroy = transport_shm_destroy;

    event_manager_shm_init(&transport->event_manager, transport, ring, logger);

    kb_arena_t *arena = &transport->arena;

    size_t alloc_size = buffer_size + sizeof(kb_arena_header_t);

    // Create shmem file descriptor
    transport->shm_fd = memfd_create(transport->name, 0);
    if (transport->shm_fd == -1) {
        perror("memfd_create failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    // Set the size
    if (ftruncate(transport->shm_fd, alloc_size) == -1)
    {
        perror("ftruncate failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    // Map the shared memory
    void *map_addr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, transport->shm_fd, 0);
    if (map_addr == MAP_FAILED)
    {
        perror("mmap failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    arena->addr = map_addr + sizeof(kb_arena_header_t);
    arena->header = (kb_arena_header_t *)map_addr;
    kb_arena_header_t *header = arena->header;

    header->write_offset = 0;
    header->read_offset = 0;
    header->size = buffer_size;
    atomic_init(&header->num_messages, 0);

    if (sem_init(&header->write_sem, 1, 1) == -1)
    {
        perror("sem_init failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    if (sem_init(&header->read_sem, 1, 1) == -1)
    {
        perror("sem_init failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    return (kb_transport_t *)transport;
}

kb_transport_t *transport_shm_connect(const char *name, int fd, struct io_uring *ring, log4c_category_t *logger)
{
    kb_transport_shm_t *transport = calloc(1, sizeof(kb_transport_shm_t));
    if (transport == NULL)
    {
        perror("calloc failed");
        return NULL;
    }

    transport->name = strdup(name);
    transport->max_message_size = 0;
    transport->shm_fd = fd;

    transport->base.message_init = transport_shm_message_init;
    transport->base.message_receive = transport_shm_message_receive;
    transport->base.get_fd = transport_shm_get_fd;
    transport->base.destroy = transport_shm_destroy;

    event_manager_shm_init(&transport->event_manager, transport, ring, logger);

    kb_arena_t *arena = &transport->arena;

    // Read mapping size
    void *map_addr = mmap(NULL, sizeof(kb_arena_header_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_addr == MAP_FAILED)
    {
        perror("Header mmap failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    kb_arena_header_t *arena_header = (kb_arena_header_t *)map_addr;
    size_t mmap_size = arena_header->size + sizeof(kb_arena_header_t);
    munmap(map_addr, sizeof(kb_arena_header_t));

    // Map the arena. Now for real
    map_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_addr == MAP_FAILED)
    {
        perror("mmap failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    arena->addr = map_addr + sizeof(kb_arena_header_t);
    arena->header = (kb_arena_header_t *)map_addr;
    kb_arena_header_t *header = arena->header;

    return (kb_transport_t*)transport;
}

kb_message_writer_t* transport_shm_message_init(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    kb_arena_t *arena = &self->arena;
    kb_arena_header_t *arena_header = arena->header;

    size_t memory_needed = self->max_message_size + sizeof(kb_message_header_t);

    sem_wait(&arena_header->write_sem);
    void *memory_chunk = NULL;

    // In case the reader read all the messages, we reset last message pointer
    if (atomic_load(&arena_header->num_messages) == 0)
    {
        self->last_message = NULL;
    }

    // Full ring
    if (arena_header->write_offset == arena_header->read_offset && self->last_message != NULL)
    {
        sem_post(&arena_header->write_sem);
        return NULL;
    }
    // Write position in at the end of the ring (can be empty if both values == arena->addr)
    else if (arena_header->write_offset >= arena_header->read_offset)
    {
        // Check if we have enough space to write the message at the end of the region
        if (arena_header->size - arena_header->write_offset >= memory_needed)
        {
            memory_chunk = arena->addr + arena_header->write_offset;
        }
        // No space available at the end of the region. Try from the start
        else
        {
            arena_header->write_offset = 0;
            sem_post(&arena_header->write_sem);
            return transport_shm_message_init(transport);
        }
    }
    // Write position is in the beginning of the region. Try to fit the message before the ring head
    else if (arena_header->read_offset - arena_header->write_offset >= memory_needed)
    {
        memory_chunk = arena->addr + arena_header->write_offset;
    }
    else
    {
        sem_post(&arena_header->write_sem);
        return NULL;
    }

    kb_message_header_t *message_header = (kb_message_header_t*)memory_chunk;
    message_header->size = self->max_message_size;
    message_header->next_message_offset = -1;

    kb_message_writer_shm_t *writer = message_writer_shm_init(self, message_header, memory_chunk + sizeof(kb_message_header_t));
    return &writer->base;
}

int transport_shm_message_send(kb_transport_t *transport, kb_message_writer_t *writer)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;
    kb_message_writer_shm_t *shm_writer = (kb_message_writer_shm_t*)writer;

    kb_arena_t *arena = &self->arena;
    kb_arena_header_t *arena_header = arena->header;

    // Shrink the memory region to needed size
    kb_message_header_t *message_header = shm_writer->header;
    message_header->size = writer_message_size(writer);

    // Expand the ring by appending message to it
    if (self->last_message != NULL)
    {
        self->last_message->next_message_offset = (void *)message_header - (void *)arena->addr;
    }

    self->last_message = message_header;

    // Move the write offset
    arena_header->write_offset += message_header->size + sizeof(kb_message_header_t);

    atomic_fetch_add(&arena_header->num_messages, 1);

    // Allow writing new messages
    sem_post(&arena_header->write_sem);

    event_manager_shm_signal_new_message(&self->event_manager);
}

kb_message_t *transport_shm_message_receive(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;
    kb_arena_t *arena = &self->arena;
    kb_arena_header_t *arena_header = arena->header;

    if (atomic_load(&arena_header->num_messages) == 0)
    {
        return NULL;
    }

    sem_wait(&arena_header->read_sem);
    kb_message_header_t *message_header = (kb_message_header_t *)(arena->addr + arena_header->read_offset);
    if (message_header == NULL)
    {
        sem_post(&arena_header->read_sem);
        return NULL;
    }

    kb_message_shm_t *message = message_shm_init(self, message_header, (char *)message_header + sizeof(kb_message_header_t));

    return &message->base;
}

int transport_shm_message_release(kb_transport_t *transport, kb_message_t *message)
{
    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;
    kb_arena_t *arena = &self->arena;
    kb_arena_header_t *arena_header = arena->header;

    kb_message_shm_t *shm_message = (kb_message_shm_t *)message;
    kb_message_header_t *message_header = shm_message->header;

    sem_wait(&arena_header->write_sem);
    if (message_header->next_message_offset != -1)
    {
        arena_header->read_offset = message_header->next_message_offset;
    }
    else
    {
        self->last_message = NULL;
        arena_header->read_offset = 0;

        arena_header->write_offset = 0;
    }
    sem_post(&arena_header->write_sem);

    atomic_fetch_sub(&arena_header->num_messages, 1);

    sem_post(&arena_header->read_sem);

    return 0;
}

int transport_shm_get_fd(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;

    return self->shm_fd;
}

void transport_shm_destroy(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    if (self->name != NULL) {
        free((void*)self->name);
    }

    kb_arena_t *arena = &self->arena;
    if (arena->addr != NULL) {
        munmap(arena->addr, arena->header->size + sizeof(kb_arena_header_t));
    }

    if (self->shm_fd != 0) {
        close(self->shm_fd);
    }

    free(transport);
}