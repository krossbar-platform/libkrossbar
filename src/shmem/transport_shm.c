#include "transport_shm.h"

#include <sys/mman.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "message_writer_shm.h"

kb_transport_t* transport_shm_init(const char *name, size_t buffer_size, size_t message_size)
{
    kb_transport_shm_t* transport = calloc(1, sizeof(kb_transport_shm_t));
    if (transport == NULL) {
        perror("calloc failed");
        return NULL;
    }

    transport->name = strdup(name);
    transport->message_size = message_size;
    kb_arena_t *arena = &transport->arena;
    arena->size = buffer_size;

    size_t alloc_size = buffer_size + sizeof(kb_arena_header_t);

    // Create shmem file descriptor
    transport->shm_fd = memfd_create(transport->name, 0);
    if (transport->shm_fd == -1) {
        perror("memfd_create failed");
        transport_shm_destroy((kb_transport_t *)transport);
        return NULL;
    }

    // Set the size
    if (ftruncate(transport->shm_fd, alloc_size) == -1) {
        perror("ftruncate failed");
        transport_shm_destroy((kb_transport_t *)transport);
        return NULL;
    }

    // Map the shared memory
    void *map_addr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, transport->shm_fd, 0);
    if (transport->arena.addr == MAP_FAILED) {
        perror("mmap failed");
        transport_shm_destroy((kb_transport_t *)transport);
        return NULL;
    }

    arena->addr = map_addr + sizeof(kb_arena_header_t);
    arena->header = (kb_arena_header_t*)map_addr;
    kb_arena_header_t *header = arena->header;

    header->write_offset = 0;
    header->read_offset = 0;

    if (sem_init(&header->write_sem, 1, 1) == -1) {
        perror("sem_init failed");
        transport_shm_destroy((kb_transport_t *)transport);
        return NULL;
    }

    if (sem_init(&header->read_sem, 1, 1) == -1) {
        perror("sem_init failed");
        transport_shm_destroy((kb_transport_t *)transport);
        return NULL;
    }

    return (kb_transport_t*)transport;
}

int transport_shm_get_fd(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    return self->shm_fd;
}

kb_message_writer_t* transport_shm_message_init(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    kb_arena_t *arena = &self->arena;
    kb_arena_header_t *arena_header = arena->header;

    size_t memory_needed = self->message_size + sizeof(kb_message_header_t);

    sem_wait(&arena_header->write_sem);
    void *memory_chunk = NULL;
    // Check if we have enough space to write the message at the end of the region
    if (arena->size - arena_header->write_offset >= memory_needed) {
        memory_chunk = arena->addr + arena_header->write_offset;
    }
    // Check if instead have enough space to write the message at the beginning of the region
    else if (arena_header->read_offset >= memory_needed) {
        memory_chunk = arena->addr;
    }
    // No space available
    else {
        sem_post(&arena_header->write_sem);
        return NULL;
    }

    kb_message_header_t *message_header = (kb_message_header_t*)memory_chunk;
    message_header->size = self->message_size;
    message_header->next_message = NULL;

    message_writer_shm_init(self, message_header, memory_chunk + sizeof(kb_message_header_t));
}

int transport_shm_message_send(kb_transport_t *transport, kb_message_writer_t *writer)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;
    kb_message_writer_shm_t *shm_writer = (kb_message_writer_shm_t*)writer;

    kb_arena_t *arena = &self->arena;
    kb_arena_header_t *arean_header = arena->header;

    // Shrink the memory region to needed size
    kb_message_header_t *message_header = shm_writer->header;
    message_header->size = writer_message_size(writer);

    // Update next message pointer. Lock from reading previous message to append message to the ring
    sem_wait(&arean_header->read_sem);
    kb_message_header_t *prev_message_header = (kb_message_header_t*)(arena->addr + arean_header->read_offset);

    if (prev_message_header != NULL) {
        prev_message_header->next_message = message_header;
    }
    sem_post(&arean_header->read_sem);

    // Move the write offset
    arean_header->write_offset += message_header->size + sizeof(kb_message_header_t);

    // Allow writing new messages
    sem_post(&arean_header->read_sem);
}

void transport_shm_destroy(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    if (self->name != NULL) {
        free((void*)self->name);
    }

    kb_arena_t *arena = &self->arena;
    if (arena->addr != NULL) {
        munmap(arena->addr, arena->size);
    }

    if (self->shm_fd != 0) {
        close(self->shm_fd);
    }

    free(transport);
}