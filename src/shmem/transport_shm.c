#include "transport_shm.h"

#include <assert.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <liburing.h>

#include "common.h"
#include "message_writer_shm.h"
#include "message_shm.h"

#define RING_QUEUE_DEPTH 32

#define MESSAGE_HEADER_SIZE (ALIGN(sizeof(kb_message_header_t)))
#define ARENA_HEADER_SIZE (ALIGN(sizeof(kb_arena_header_t)))

int transport_shm_create_mapping(const char *name, size_t buffer_size, log4c_category_t *logger)
{
    // Create shmem file descriptor
    int result_fd = memfd_create(name, 0);
    if (result_fd == -1)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "memfd_create failed: %s", strerror(errno));
        return -1;
    }

    size_t alloc_size = buffer_size + ARENA_HEADER_SIZE;
    // Set the size
    if (ftruncate(result_fd, alloc_size) == -1)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "ftruncate failed: %s", strerror(errno));
        return -1;
    }

    // Map the shared memory to set the header up
    void *map_addr = mmap(NULL, ARENA_HEADER_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, result_fd, 0);
    if (map_addr == MAP_FAILED)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "mmap failed: %s", strerror(errno));
        close(result_fd);
        return -1;
    }

    kb_arena_header_t *arena_header = map_addr;
    arena_header->first_message_offset = NULL_OFFSET;
    arena_header->last_message_offset = NULL_OFFSET;
    arena_header->size = buffer_size;
    atomic_init(&arena_header->num_messages, 0);

    return result_fd;
}

size_t transport_shm_get_mapping_size(int map_fd, log4c_category_t *logger)
{
    struct stat stat;
    if (fstat(map_fd, &stat) == -1)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "fstat failed: %s", strerror(errno));
        return 0;
    }

    return stat.st_size - ARENA_HEADER_SIZE;
}

kb_transport_t *transport_shm_init(const char *name, int read_fd, int write_fd,
                                   size_t max_message_size, struct io_uring *ring, log4c_category_t *logger)
{
    assert(name != NULL);
    assert(ring != NULL);
    assert(logger != NULL);

    kb_transport_shm_t *transport = malloc(sizeof(kb_transport_shm_t));
    if (transport == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "malloc failed");
        return NULL;
    }

    transport->base.name = strdup(name);
    transport->base.logger = logger;
    transport->base.message_init = transport_shm_message_init;
    transport->base.message_receive = transport_shm_message_receive;
    transport->base.destroy = transport_shm_destroy;

    kb_event_manager_shm_t *event_manager = event_manager_shm_create(transport, ring, logger);
    if (event_manager == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "malloc failed");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    transport->base.event_manager = (kb_event_manager_t *)event_manager;

    // Map receiving memory mapping
    size_t read_mapping_size = transport_shm_get_mapping_size(read_fd, logger);
    void *map_read_addr = mmap(NULL, read_mapping_size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, read_fd, 0);
    if (map_read_addr == MAP_FAILED)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Read mmap failed: %s", strerror(errno));
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    kb_allocator_t *read_allocator = allocator_attach(map_read_addr, logger);
    if (read_allocator == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to attach to read allocator");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    transport->read_arena.header = (kb_arena_header_t *)map_read_addr;
    transport->read_arena.allocator = read_allocator;
    transport->read_arena.shm_fd = read_fd;
    transport->max_message_size = max_message_size;

    // Map sending memory mapping
    size_t write_mapping_size = transport_shm_get_mapping_size(write_fd, logger);

    if (write_mapping_size < max_message_size)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Write mapping size is smaller than max message size: %zu < %zu",
                           write_mapping_size, max_message_size);
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    void *map_write_addr = mmap(NULL, write_mapping_size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, write_fd, 0);
    if (map_write_addr == MAP_FAILED)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Write mmap failed: %s", strerror(errno));
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    size_t mesage_allocation_size = max_message_size + ALIGN(MESSAGE_HEADER_SIZE);
    kb_allocator_t *write_allocator = allocator_create(map_write_addr, write_mapping_size, mesage_allocation_size, logger);
    if (write_allocator == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to create write allocator");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    transport->write_arena.header = (kb_arena_header_t *)map_write_addr;
    transport->write_arena.allocator = write_allocator;
    transport->write_arena.shm_fd = write_fd;

    log4c_category_log(logger, LOG4C_PRIORITY_DEBUG, "Shared memory transport `%s` initialized", name);

    return (kb_transport_t *)transport;
}

kb_message_header_t *transport_message_from_offset(kb_arena_t *arena, size_t offset)
{
    if (offset == NULL_OFFSET)
    {
        return NULL;
    }

    return (kb_message_header_t *)(arena->header + offset);
}

size_t transport_message_offset(kb_arena_t *arena, kb_message_header_t *message_header)
{
    if (message_header == NULL)
    {
        return NULL_OFFSET;
    }

    return (char *)message_header - (char *)arena->header;
}

kb_message_writer_t *transport_shm_message_init(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;

    void *memory_chunk = allocator_alloc(self->write_arena.allocator);
    if (memory_chunk == NULL)
    {
        return NULL;
    }

    kb_arena_t *arena = &self->write_arena;
    kb_arena_header_t *arena_header = arena->header;

    kb_message_header_t *message_header = (kb_message_header_t *)memory_chunk;
    message_header->size = self->max_message_size;
    message_header->next_message_offset = NULL_OFFSET;

    kb_message_writer_shm_t *writer = message_writer_shm_init(self, message_header, memory_chunk + MESSAGE_HEADER_SIZE);
    return &writer->base;
}

int transport_shm_message_send(kb_transport_t *transport, kb_message_writer_t *writer)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;
    kb_message_writer_shm_t *shm_writer = (kb_message_writer_shm_t*)writer;

    kb_arena_t *arena = &self->write_arena;
    kb_arena_header_t *arena_header = arena->header;

    // Locking here also prevents from reading the mesage list
    allocator_lock(arena->allocator);

    // Release extra memory
    kb_message_header_t *message_header = shm_writer->header;
    message_header->size = writer_message_size(writer);
    allocator_trim_block(arena->allocator, (kb_block_header_t *)(message_header + MESSAGE_HEADER_SIZE), message_header->size - MESSAGE_HEADER_SIZE, false);

    size_t message_offset = transport_message_offset(arena, message_header);

    // Append mesage to the messages list
    size_t last_message_offset = arena_header->last_message_offset;
    if (last_message_offset != NULL_OFFSET)
    {
        kb_message_header_t *last_buffered_message = transport_message_from_offset(arena, last_message_offset);
        last_buffered_message->next_message_offset = message_offset;
    }
    else
    {
        // No message in the list means we also need to replace the list head
        arena_header->first_message_offset = message_offset;
    }

    atomic_store(&arena_header->last_message_offset, message_offset);
    allocator_unlock(arena->allocator);

    uint32_t num_mesages = atomic_fetch_add(&arena_header->num_messages, 1);
    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "New shmem message in `%s`: %d messages in the buffer", transport->name, num_mesages + 1);

    event_manager_shm_signal_new_message((kb_event_manager_shm_t *)transport->event_manager);
}

kb_message_t *transport_shm_message_receive(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;
    kb_arena_t *arena = &self->read_arena;
    kb_arena_header_t *arena_header = arena->header;

    if (atomic_load(&arena_header->num_messages) == 0)
    {
        return NULL;
    }

    allocator_lock(arena->allocator);
    kb_message_header_t *incoming_message = transport_message_from_offset(arena, arena_header->first_message_offset);
    // Having NULL here means that the message was already removed by another thread
    assert(incoming_message != NULL);

    // Remove the message from the list
    arena_header->first_message_offset = incoming_message->next_message_offset;
    if (arena_header->first_message_offset == NULL_OFFSET)
    {
        // No more messages in the list
        arena_header->last_message_offset = NULL_OFFSET;
    }
    allocator_unlock(arena->allocator);
    uint32_t num_mesages = atomic_fetch_sub(&arena_header->num_messages, 1);
    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "Removed shmem message from `%s`: %d messages in the buffer", transport->name, num_mesages - 1);

    kb_message_shm_t *message = message_shm_init(self, incoming_message, (char *)incoming_message + MESSAGE_HEADER_SIZE);

    return &message->base;
}

int transport_shm_message_release(kb_transport_t *transport, kb_message_t *message)
{
    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;
    kb_arena_t *arena = &self->read_arena;

    allocator_free(arena->allocator, (char *)message + MESSAGE_HEADER_SIZE);

    return 0;
}

void transport_shm_destroy(kb_transport_t *transport)
{
    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "Shared memory transport `%s` destroyed", transport->name);

    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    if (transport->name != NULL)
    {
        free((void *)transport->name);
    }

    kb_allocator_t *read_allocator = self->read_arena.allocator;
    if (read_allocator != NULL)
    {
        munmap(read_allocator->header - ARENA_HEADER_SIZE, read_allocator->header->total_size + ARENA_HEADER_SIZE);
        close(self->read_arena.shm_fd);
        allocator_destroy(read_allocator);
    }

    kb_allocator_t *write_allocator = self->write_arena.allocator;
    if (write_allocator != NULL)
    {
        munmap(write_allocator->header - ARENA_HEADER_SIZE, write_allocator->header->total_size + ARENA_HEADER_SIZE);
        close(self->write_arena.shm_fd);
        allocator_destroy(write_allocator);
    }

    event_manager_shm_destroy((kb_event_manager_shm_t *)transport->event_manager);

    free(transport);
}