#include "transport_shm.h"

#include <assert.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <mpack-reader.h>

#include <liburing.h>

#include "common.h"
#include "message_writer_shm.h"
#include "message_shm.h"
#include "../utils.h"

#define RING_QUEUE_DEPTH 32

#define MESSAGE_HEADER_SIZE (ALIGN(sizeof(kb_message_header_t)))
#define ARENA_HEADER_SIZE (ALIGN(sizeof(kb_arena_header_t)))

static void arena_lock(kb_arena_t *arena)
{
    assert(arena != NULL);

    const uint32_t zero = 0;

    while (true)
    {
        // Check if the futex is available
        if (atomic_compare_exchange_strong(&arena->header->futex, &zero, 1))
        {
            break;
        }

        // Futex is not available; wait
        long result = futex_wait(&arena->header->futex, 1);
        if (result == -1 && errno != EAGAIN)
        {
            perror("Failed to wait on futex");
            exit(1);
        }
    }
}

static void arena_unlock(kb_arena_t *arena)
{
    assert(arena != NULL);

    const uint32_t one = 1;

    if (atomic_compare_exchange_strong(&arena->header->futex, &one, 0))
    {
        int result = futex_wake(&arena->header->futex, INT_MAX);
        if (result == -1)
        {
            perror("Failed to wake futex");
            exit(1);
        }
    }
}

int transport_shm_create_mapping(const char *name, size_t buffer_size, log4c_category_t *logger)
{
    assert(name != NULL);
    assert(buffer_size > 0);
    assert(logger != NULL);
    log4c_category_log(logger, LOG4C_PRIORITY_DEBUG, "Creating shared memory mapping `%s` of size %zu", name, buffer_size);

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

    log_trace(logger, "Shared memory arena `%s` created at %p", name, map_addr);

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
    assert(read_fd >= 0);
    assert(write_fd >= 0);

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

    kb_allocator_t *read_allocator = allocator_attach(
        OFFSET_POINTER(map_read_addr, ARENA_HEADER_SIZE), logger);
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

    log_trace(logger, "Shared memory read arena `%s` mapped at %p", name, map_read_addr);

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

    kb_allocator_t *write_allocator = allocator_create(
        OFFSET_POINTER(map_write_addr, ARENA_HEADER_SIZE),
        write_mapping_size - ARENA_HEADER_SIZE,
        max_message_size + MESSAGE_HEADER_SIZE, logger);
    if (write_allocator == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Failed to create write allocator");
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    transport->write_arena.header = (kb_arena_header_t *)map_write_addr;
    transport->write_arena.allocator = write_allocator;
    transport->write_arena.shm_fd = write_fd;

    log_trace(logger, "Shared memory write arena `%s` mapped at %p", name, map_write_addr);

    return (kb_transport_t *)transport;
}

kb_message_header_t *transport_message_from_offset(kb_arena_t *arena, size_t offset)
{
    assert(arena != NULL);
    assert(offset == NULL_OFFSET ? true : offset < arena->header->size && offset > 0 && offset % ALIGNMENT == 0);

    if (offset == NULL_OFFSET)
    {
        return NULL;
    }

    return OFFSET_POINTER(arena->header, offset);
}

size_t transport_message_offset(kb_arena_t *arena, kb_message_header_t *message_header)
{
    assert(arena != NULL);

    if (message_header == NULL)
    {
        return NULL_OFFSET;
    }

    return (char *)message_header - (char *)arena->header;
}

kb_message_writer_t *transport_shm_message_init(kb_transport_t *transport)
{
    assert(transport != NULL);

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

    kb_message_writer_shm_t *writer = message_writer_shm_init(self, message_header,
                                                              OFFSET_POINTER(memory_chunk, MESSAGE_HEADER_SIZE));
    return &writer->base;
}

int transport_shm_message_send(kb_transport_t *transport, kb_message_writer_t *writer)
{
    assert(transport != NULL);
    assert(writer != NULL);

    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;
    kb_message_writer_shm_t *shm_writer = (kb_message_writer_shm_t *)writer;

    kb_arena_t *arena = &self->write_arena;
    kb_arena_header_t *arena_header = arena->header;

    // Release extra memory
    kb_message_header_t *message_header = shm_writer->header;
    message_header->size = writer_message_size(writer);
    allocator_trim(arena->allocator, message_header, message_header->size + MESSAGE_HEADER_SIZE);

    size_t message_offset = transport_message_offset(arena, message_header);

    // Locking here also prevents from reading the mesage list
    arena_lock(arena);
    // Append mesage to the messages list
    size_t last_message_offset = arena_header->last_message_offset;
    // In case we have a mesage in the buffer, we change it's pointer to point to the incoming message
    if (last_message_offset != NULL_OFFSET)
    {
        kb_message_header_t *last_message = transport_message_from_offset(arena, last_message_offset);
        last_message->next_message_offset = message_offset;
    }
    // In case of en empty list, we need to set the first message pointer
    // The last message pointer will be update after the `if` block
    else
    {
        // No message in the list means we also need to replace the list head
        arena_header->first_message_offset = message_offset;
    }

    atomic_store(&arena_header->last_message_offset, message_offset);
    arena_unlock(arena);

    uint32_t num_mesages = atomic_fetch_add(&arena_header->num_messages, 1);
    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "New shmem message in `%s`: %d messages in the buffer", transport->name, num_mesages + 1);
    log_trace(transport->logger, "Written message offset %zd. Pointer: %p. Size: %zd", message_offset, message_header, message_header->size);
    log_trace(transport->logger, "New mesage list: first: %zd, last: %zd", arena_header->first_message_offset, arena_header->last_message_offset);

    event_manager_shm_signal_new_message((kb_event_manager_shm_t *)transport->event_manager);
}

kb_message_t *transport_shm_message_receive(kb_transport_t *transport)
{
    assert(transport != NULL);

    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;
    kb_arena_t *arena = &self->read_arena;
    kb_arena_header_t *arena_header = arena->header;
    log_trace(transport->logger, "READ Arena header data: %zd %zd %zd", arena_header->size, arena_header->first_message_offset, arena_header->last_message_offset);

    if (atomic_load(&arena_header->num_messages) == 0)
    {
        return NULL;
    }

    kb_message_header_t *incoming_message = transport_message_from_offset(arena, arena_header->first_message_offset);
    // Having NULL here means that the message was already removed by another thread
    assert(incoming_message != NULL);

    log_trace(transport->logger, "Read message offset %zd. Pointer: %p. Size: %zd", arena_header->first_message_offset, incoming_message, incoming_message->size);

    arena_lock(arena);
    // Remove the message from the list
    arena_header->first_message_offset = incoming_message->next_message_offset;
    if (arena_header->first_message_offset == NULL_OFFSET)
    {
        // No more messages in the list. Also nullify the last mesage offset
        arena_header->last_message_offset = NULL_OFFSET;
    }

    arena_unlock(arena);
    uint32_t num_mesages = atomic_fetch_sub(&arena_header->num_messages, 1);

    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "Removed shmem message from `%s`: %d messages in the buffer", transport->name, num_mesages - 1);
    log_trace(transport->logger, "New mesage list: first: %zd, last: %zd", arena_header->first_message_offset, arena_header->last_message_offset);

    kb_message_shm_t *message = message_shm_init(self, incoming_message,
                                                 OFFSET_POINTER(incoming_message, MESSAGE_HEADER_SIZE));

    return &message->base;
}

int transport_shm_message_release(kb_transport_t *transport, kb_message_t *message)
{
    assert(transport != NULL);
    assert(message != NULL);

    kb_transport_shm_t *self = (kb_transport_shm_t *)transport;
    kb_arena_t *arena = &self->read_arena;

    kb_message_shm_t *shm_message = (kb_message_shm_t *)message;
    kb_message_header_t *message_header = shm_message->header;

    log_trace(transport->logger, "Releasing message %p with offset %zd", message, transport_message_offset(arena, message_header));
    allocator_free(arena->allocator, message_header);

    return 0;
}

void transport_shm_destroy(kb_transport_t *transport)
{
    assert(transport != NULL);

    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "Shared memory transport `%s` destroyed", transport->name);

    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    if (transport->name != NULL)
    {
        free((void *)transport->name);
    }

    kb_allocator_t *read_allocator = self->read_arena.allocator;
    if (read_allocator != NULL)
    {
        munmap(OFFSET_POINTER(read_allocator->header, -ARENA_HEADER_SIZE),
               read_allocator->header->total_size + ARENA_HEADER_SIZE);
        close(self->read_arena.shm_fd);
        allocator_destroy(read_allocator);
    }

    kb_allocator_t *write_allocator = self->write_arena.allocator;
    if (write_allocator != NULL)
    {
        munmap(OFFSET_POINTER(write_allocator->header, -ARENA_HEADER_SIZE),
               write_allocator->header->total_size + ARENA_HEADER_SIZE);
        close(self->write_arena.shm_fd);
        allocator_destroy(write_allocator);
    }

    event_manager_shm_destroy((kb_event_manager_shm_t *)transport->event_manager);

    free(transport);
}