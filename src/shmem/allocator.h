#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <log4c/category.h>

struct kb_block_header_s
{
    size_t size;              // Size of the block (including header)
    bool is_free;             // Whether the block is free
    size_t prev_offset;       // Offset of the previous block in the list
    size_t next_offset;       // Offset of the next block in the list
};

typedef struct kb_block_header_s kb_block_header_t;

struct kb_allocator_header_s
{
    uint32_t futex;                  // Futex for synchronization
    size_t total_size;               // Total size of the shared memory region
    size_t used_size;                // Currently used size
    size_t first_block_offset;       // Offset of the first free block
    size_t last_block_offset;        // Offset of the last free block
};

typedef struct kb_allocator_header_s kb_allocator_header_t;

// Allocator structure (shared memory region header)
struct kb_allocator_s
{
    kb_allocator_header_t *header;   // Header for the shared memory region
    log4c_category_t *logger;        // Logger for debugging
    size_t max_message_size;         // Maximum message size for initial allocations
};

typedef struct kb_allocator_s kb_allocator_t;

kb_allocator_t *allocator_create(void *memory, size_t total_size, size_t max_message_size, log4c_category_t *logger);
/**
 * Allocate a block for a message
 * Always allocates the maximum message size initially
 *
 * @param allocator Pointer to the allocator
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void *allocator_alloc(kb_allocator_t *allocator);
void allocator_free(kb_allocator_t *allocator, void *ptr);
void allocator_destroy(kb_allocator_t *allocator);

void allocator_lock(kb_allocator_t *allocator);
void allocator_unlock(kb_allocator_t *allocator);
size_t allocator_block_offset(kb_allocator_t *allocator, kb_block_header_t *block);
kb_block_header_t *allocator_offset_to_block(kb_allocator_t *allocator, size_t offset);

void allocator_add_free_block(kb_allocator_t *allocator, kb_block_header_t *block);
void allocator_remove_free_block(kb_allocator_t *allocator, kb_block_header_t *block);
void allocator_coalesce_free_blocks(kb_allocator_t *allocator, kb_block_header_t *block);
void allocator_trim_block(kb_allocator_t *allocator, kb_block_header_t *block, size_t actual_size, bool lock);

void allocator_dump(kb_allocator_t *allocator);
