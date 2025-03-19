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

int transport_shm_create_mapping(const char *name, size_t buffer_size, log4c_category_t *logger)
{
    // Create shmem file descriptor
    int result_fd = memfd_create(name, 0);
    if (result_fd == -1)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "memfd_create failed: %s", strerror(errno));
        return -1;
    }

    size_t alloc_size = buffer_size + sizeof(kb_arena_header_t);
    // Set the size
    if (ftruncate(result_fd, alloc_size) == -1)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "ftruncate failed: %s", strerror(errno));
        return -1;
    }

    // Map the shared memory to set the header up
    void *map_addr = mmap(NULL, sizeof(kb_arena_header_t), PROT_READ | PROT_WRITE,
                          MAP_SHARED, result_fd, 0);
    if (map_addr == MAP_FAILED)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "mmap failed: %s", strerror(errno));
        close(result_fd);
        return -1;
    }

    kb_arena_header_t *arena_header = map_addr;
    arena_header->write_offset = 0;
    arena_header->read_offset = 0;
    arena_header->size = buffer_size;
    atomic_init(&arena_header->num_messages, 0);

    if (sem_init(&arena_header->write_sem, 1, 1) == -1)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "sem_init failed: %s", strerror(errno));
        munmap(map_addr, sizeof(kb_arena_header_t));
        close(result_fd);
        return -1;
    }

    if (sem_init(&arena_header->read_sem, 1, 1) == -1)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "sem_init failed: %s", strerror(errno));
        munmap(map_addr, sizeof(kb_arena_header_t));
        close(result_fd);
        return -1;
    }

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

    return stat.st_size - sizeof(kb_arena_header_t);
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

    transport->max_message_size = max_message_size;

    transport->base.name = strdup(name);
    transport->base.logger = logger;
    transport->base.message_init = transport_shm_message_init;
    transport->base.message_receive = transport_shm_message_receive;
    transport->base.destroy = transport_shm_destroy;

    event_manager_shm_init(&transport->event_manager, transport, ring, logger);

    // Map read shared memory
    size_t read_mapping_size = transport_shm_get_mapping_size(read_fd, logger);
    void *map_read_addr = mmap(NULL, read_mapping_size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, read_fd, 0);
    if (map_read_addr == MAP_FAILED)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Read mmap failed: %s", strerror(errno));
        transport_shm_destroy(&transport->base);
        return NULL;
    }

    transport->read_arena.addr = map_read_addr + sizeof(kb_arena_header_t);
    transport->read_arena.header = (kb_arena_header_t *)map_read_addr;
    transport->read_arena.shm_fd = read_fd;

    // Map write shared memory
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

    transport->write_arena.addr = map_write_addr + sizeof(kb_arena_header_t);
    transport->write_arena.header = (kb_arena_header_t *)map_write_addr;
    transport->write_arena.shm_fd = write_fd;

    log4c_category_log(logger, LOG4C_PRIORITY_DEBUG, "Shared memory transport `%s` initialized", name);

    return (kb_transport_t *)transport;
}

kb_message_writer_t* transport_shm_message_init(kb_transport_t *transport)
{
    kb_transport_shm_t *self = (kb_transport_shm_t*)transport;

    kb_arena_t *arena = &self->write_arena;
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

    kb_arena_t *arena = &self->write_arena;
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

    uint32_t num_mesages = atomic_fetch_add(&arena_header->num_messages, 1);

    // Allow writing new messages
    sem_post(&arena_header->write_sem);
    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "New shmem message in `%s`: %d messages in the buffer", transport->name, num_mesages + 1);

    event_manager_shm_signal_new_message(&self->event_manager);
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
    kb_arena_t *arena = &self->read_arena;
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

    uint32_t num_mesages = atomic_fetch_sub(&arena_header->num_messages, 1);

    sem_post(&arena_header->read_sem);
    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "Removed shmem message from `%s`: %d messages in the buffer", transport->name, num_mesages - 1);

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

    if (self->read_arena.addr != NULL)
    {
        munmap(self->read_arena.addr, self->read_arena.header->size + sizeof(kb_arena_header_t));
        close(self->read_arena.shm_fd);
    }

    if (self->write_arena.addr != NULL)
    {
        munmap(self->write_arena.addr, self->write_arena.header->size + sizeof(kb_arena_header_t));
        close(self->write_arena.shm_fd);
    }

    free(transport);
}