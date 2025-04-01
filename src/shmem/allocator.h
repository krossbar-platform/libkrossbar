#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <log4c/category.h>

enum kb_block_type_e
{
    KB_BLOCK_TAG_FREE = 0x0,
    KB_BLOCK_TAG_ALLOCATED = 0x1
};

typedef enum kb_block_type_e kb_block_type_t;

struct kb_block_header_s
{
    size_t size;                   // Size of the block (including tags)
    kb_block_type_t type;          // Free or allocated
    size_t next_free_block_offset; // Offset of the next block in the list
};

typedef struct kb_block_header_s kb_block_header_t;

struct kb_block_footer_s
{
    size_t size;          // Size of the block (including tags)
    kb_block_type_t type; // Free or allocated
};

typedef struct kb_block_footer_s kb_block_footer_t;

struct kb_allocator_header_s
{
    uint32_t futex;                  // Futex for synchronization
    size_t total_size;               // Total size of the shared memory region
    size_t free_size;                // Currently used size
    size_t next_free_block_offset;   // Offset of the first free block
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
void allocator_destroy(kb_allocator_t *allocator);
/**
 * Allocate a block for a message
 * Always allocates the maximum message size initially
 *
 * @param allocator Pointer to the allocator
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void *allocator_alloc(kb_allocator_t *allocator);
void allocator_free(kb_allocator_t *allocator, void *ptr);

void allocator_lock(kb_allocator_t *allocator);
void allocator_unlock(kb_allocator_t *allocator);
size_t allocator_block_offset(kb_allocator_t *allocator, kb_block_header_t *block);
kb_block_header_t *allocator_offset_to_block(kb_allocator_t *allocator, size_t offset);
void *allocator_write_block_tags(kb_allocator_t *allocator, kb_block_header_t *block, size_t size, kb_block_type_t type);

void allocator_add_free_block(kb_allocator_t *allocator, kb_block_header_t *block);
void allocator_remove_free_block(kb_allocator_t *allocator, kb_block_header_t *block);
kb_block_header_t *allocator_prev_adjacent_free_block(kb_allocator_t *allocator, kb_block_header_t *block);
kb_block_header_t *allocator_next_adjacent_free_block(kb_allocator_t *allocator, kb_block_header_t *block);
void allocator_coalesce_free_blocks(kb_allocator_t *allocator, kb_block_header_t *block);
void allocator_trim_block(kb_allocator_t *allocator, kb_block_header_t *block, size_t actual_size, bool lock);

void allocator_dump(kb_allocator_t *allocator);
