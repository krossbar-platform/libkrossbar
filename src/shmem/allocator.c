#include "allocator.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "../utils.h"

// Block header size
#define BLOCK_HEADER_SIZE ALIGN(sizeof(kb_block_header_t))
// Block footer size
#define BLOCK_FOOTER_SIZE ALIGN(sizeof(kb_block_footer_t))
// Minimum block size (including header and footer)
#define MIN_BLOCK_SIZE (BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE + 64)

static void allocator_add_free_block(kb_allocator_t *allocator, kb_block_header_t *block)
{
    assert(allocator != NULL);
    assert(block != NULL);

    allocator_write_block_tags(allocator, block, block->size, KB_BLOCK_TAG_FREE);
    kb_allocator_header_t *alloc_header = allocator->header;

    size_t block_offset = allocator_block_offset(allocator, block);

    // Put the new block at the head of the free list
    block->next_free_block_offset = alloc_header->next_free_block_offset;
    alloc_header->next_free_block_offset = block_offset;
}

static void allocator_remove_free_block(kb_allocator_t *allocator, kb_block_header_t *block)
{
    assert(allocator != NULL);
    assert(block != NULL);

    kb_allocator_header_t *alloc_header = allocator->header;
    assert(alloc_header->next_free_block_offset != NULL_OFFSET);

    size_t block_offset = allocator_block_offset(allocator, block);

    // It's the first block in the free list
    if (alloc_header->next_free_block_offset == block_offset)
    {
        alloc_header->next_free_block_offset = block->next_free_block_offset;
        block->next_free_block_offset = NULL_OFFSET;
        allocator_write_block_tags(allocator, block, block->size, KB_BLOCK_TAG_ALLOCATED);
        return;
    }

    // Somewhere in the middle of the list. Let's search
    kb_block_header_t *current_block = allocator_offset_to_block(allocator, alloc_header->next_free_block_offset);
    while (current_block != NULL)
    {
        // Check if the next block is the one we're looking for
        if (current_block->next_free_block_offset == block_offset)
        {
            current_block->next_free_block_offset = block->next_free_block_offset;
            block->next_free_block_offset = NULL_OFFSET;
            allocator_write_block_tags(allocator, block, block->size, KB_BLOCK_TAG_ALLOCATED);
            return;
        }

        current_block = allocator_offset_to_block(allocator, current_block->next_free_block_offset);
    }

    assert(false);
}

static void allocator_lock(kb_allocator_t *allocator)
{
    assert(allocator != NULL);

    const uint32_t zero = 0;

    while (true)
    {
        // Check if the futex is available
        if (atomic_compare_exchange_strong(&allocator->header->futex, &zero, 1))
        {
            break;
        }

        // Futex is not available; wait
        long result = futex_wait(&allocator->header->futex, 1);
        if (result == -1 && errno != EAGAIN)
        {
            log4c_category_error(allocator->logger, "Failed to wait on futex: %s", strerror(errno));
            exit(1);
        }
    }
}

static void allocator_unlock(kb_allocator_t *allocator)
{
    assert(allocator != NULL);

    const uint32_t one = 1;

    if (atomic_compare_exchange_strong(&allocator->header->futex, &one, 0))
    {
        int result = futex_wake(&allocator->header->futex, INT_MAX);
        if (result == -1)
        {
            log4c_category_error(allocator->logger, "Failed to wake futex: %s", strerror(errno));
            exit(1);
        }
    }
}

kb_allocator_t *allocator_create(void *memory, size_t total_size, size_t max_message_size, log4c_category_t *logger)
{
    assert(memory != NULL);
    assert(logger != NULL);
    assert(total_size > 0);
    assert(max_message_size > 0);
    assert(total_size >= max_message_size);

    log_trace(logger, "Creating allocator at %p", memory);

    // Ensure the memory region is large enough for the allocator structure and at least one block
    if (total_size < sizeof(kb_allocator_t) + ALIGN(max_message_size) + sizeof(kb_block_header_t))
    {
        log4c_category_error(logger, "Memory region is too small for allocator");
        return NULL;
    }

    kb_allocator_t *allocator = (kb_allocator_t *)malloc(sizeof(kb_allocator_t));
    if (allocator == NULL) {
        log4c_category_error(logger, "Failed to allocate memory for allocator");
        return NULL;
    }

    // Initialize the allocator structure at the beginning of the memory region
    kb_allocator_header_t *alloc_header = (kb_allocator_header_t *)memory;
    allocator->header = alloc_header;
    allocator->logger = logger;

    size_t header_size = ALIGN(sizeof(kb_allocator_header_t));

    // Initialize the allocator header
    alloc_header->futex = 0;
    alloc_header->total_size = total_size - header_size;
    alloc_header->free_size = alloc_header->total_size;
    alloc_header->next_free_block_offset = NULL_OFFSET;
    alloc_header->max_message_size = ALIGN(max_message_size);

    log_trace(logger, "Allocator header initialized: total_size=%zu, free_size=%zu, max_message_size=%zu",
              alloc_header->total_size, alloc_header->free_size, alloc_header->max_message_size);

    // Initialize the first block
    kb_block_header_t *first_block = (kb_block_header_t*)(memory + header_size);
    first_block->next_free_block_offset = NULL_OFFSET;
    allocator_write_block_tags(allocator, first_block, alloc_header->total_size, KB_BLOCK_TAG_FREE);

    // Add the block to the free list
    allocator_add_free_block(allocator, first_block);

    return allocator;
}

kb_allocator_t *allocator_attach(void *memory, log4c_category_t *logger)
{
    assert(memory != NULL);
    assert(logger != NULL);

    log_trace(logger, "Attaching to allocator at %p", memory);

    kb_allocator_t *allocator = (kb_allocator_t *)malloc(sizeof(kb_allocator_t));
    if (allocator == NULL)
    {
        log4c_category_error(logger, "Failed to allocate memory for allocator");
        return NULL;
    }

    allocator->header = (kb_allocator_header_t *)memory;
    allocator->logger = logger;

    log_trace(logger, "Allocator header attached: total_size=%zu, free_size=%zu, max_message_size=%zu",
              allocator->header->total_size, allocator->header->free_size, allocator->header->max_message_size);

    return allocator;
}

void allocator_destroy(kb_allocator_t *allocator)
{
    assert(allocator != NULL);

    free(allocator);
}

inline size_t allocator_block_offset(kb_allocator_t *allocator, kb_block_header_t *block)
{
    assert(allocator != NULL);
    assert(block != NULL);

    return (char *)block - (char *)allocator->header;
}

kb_block_header_t *allocator_offset_to_block(kb_allocator_t *allocator, size_t offset)
{
    assert(allocator != NULL);
    assert(offset == NULL_OFFSET ? true : offset < allocator->header->total_size && offset > 0 && offset % ALIGNMENT == 0);

    if (offset == NULL_OFFSET)
    {
        return NULL;
    }

    return OFFSET_POINTER(allocator->header, offset);
}

void allocator_write_block_tags(kb_allocator_t *allocator, kb_block_header_t *block, size_t size, kb_block_type_t type)
{
    assert(allocator != NULL);
    assert(block != NULL);
    assert(size > 0);

    block->size = size;
    block->type = type;

    kb_block_footer_t *footer = OFFSET_POINTER(block, size - BLOCK_FOOTER_SIZE);
    footer->size = size;
    footer->type = type;
}

void *allocator_alloc(kb_allocator_t *allocator)
{
    assert(allocator != NULL);

    // Always allocate the maximum message size initially
    size_t alloc_size = allocator->header->max_message_size + BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE;

    allocator_lock(allocator);

    // Find a suitable free block (best fit strategy)
    kb_block_header_t *block = allocator_offset_to_block(allocator, allocator->header->next_free_block_offset);
    kb_block_header_t *best_fit = NULL;

    // Find the best fit block
    while (block)
    {
        if (block->size >= alloc_size)
        {
            best_fit = block;
            break;
        }

        block = allocator_offset_to_block(allocator, block->next_free_block_offset);
    }

    if (!best_fit)
    {
        // No suitable block found
        allocator_unlock(allocator);
        return NULL;
    }

    log_trace(allocator->logger, "Allocated block at %zd of %zu bytes", allocator_block_offset(allocator, best_fit), best_fit->size);

    // Remove the block from the free list
    allocator_remove_free_block(allocator, best_fit);
    // Split the block if it's too large
    allocator_trim_block(allocator, best_fit, alloc_size, false);

    // Mark the block as allocated
    allocator_write_block_tags(allocator, best_fit, best_fit->size, KB_BLOCK_TAG_ALLOCATED);
    allocator->header->free_size -= best_fit->size;

    allocator_unlock(allocator);

    return OFFSET_POINTER(best_fit, BLOCK_HEADER_SIZE);
}

void allocator_free(kb_allocator_t *allocator, void *ptr)
{
    assert(allocator != NULL);
    assert(ptr != NULL);

    if (ptr == NULL)
    {
        return;
    }

    allocator_lock(allocator);

    kb_block_header_t *block = OFFSET_POINTER(ptr, -BLOCK_HEADER_SIZE);
    log_trace(allocator->logger, "Freeing block at %zd", allocator_block_offset(allocator, block));

    // Update used size
    allocator->header->free_size += block->size;

    // Mark the block as free and try to coalesce it with adjacent blocks
    allocator_add_free_block(allocator, block);
    allocator_coalesce_free_blocks(allocator, block);

    allocator_unlock(allocator);
}

static kb_block_header_t *allocator_prev_adjacent_free_block(kb_allocator_t *allocator, kb_block_header_t *block)
{
    assert(allocator != NULL);
    assert(block != NULL);

    kb_block_footer_t *prev_block_footer = OFFSET_POINTER(block, -BLOCK_FOOTER_SIZE);
    if (prev_block_footer->type == KB_BLOCK_TAG_FREE)
    {
        return allocator_offset_to_block(allocator, allocator_block_offset(allocator, block) - prev_block_footer->size);
    }

    return NULL;
}

static kb_block_header_t *allocator_next_adjacent_free_block(kb_allocator_t *allocator, kb_block_header_t *block)
{
    assert(allocator != NULL);
    assert(block != NULL);

    kb_block_header_t *next_block = allocator_offset_to_block(allocator, allocator_block_offset(allocator, block) + block->size);
    if (next_block->type == KB_BLOCK_TAG_FREE)
    {
        return next_block;
    }

    return NULL;
}

// Try to coalesce a block with adjacent free blocks
void allocator_coalesce_free_blocks(kb_allocator_t *allocator, kb_block_header_t *block)
{
    assert(allocator != NULL);
    assert(block != NULL);
    assert(block->type == KB_BLOCK_TAG_FREE);

    kb_block_header_t *prev_block = allocator_prev_adjacent_free_block(allocator, block);
    if (prev_block)
    {
        // Update size for the previous block
        allocator_remove_free_block(allocator, block);
        allocator_write_block_tags(allocator, prev_block, prev_block->size += block->size, KB_BLOCK_TAG_FREE);
        block = prev_block;
    }

    kb_block_header_t *next_block = allocator_next_adjacent_free_block(allocator, block);
    if (next_block)
    {
        // Update size for the current block
        allocator_remove_free_block(allocator, next_block);
        allocator_write_block_tags(allocator, block, block->size += next_block->size, KB_BLOCK_TAG_FREE);
    }
}

void allocator_trim_block(kb_allocator_t *allocator, kb_block_header_t *block, size_t new_size, bool lock)
{
    assert(allocator != NULL);
    assert(block != NULL);
    assert(new_size > 0);
    assert(new_size % ALIGNMENT == 0);

    log_trace(allocator->logger, "Trimming block at %zd to %zu bytes", allocator_block_offset(allocator, block), new_size);

    // We only trim allocated blocks
    assert(block->type == KB_BLOCK_TAG_ALLOCATED);

    if (block->size < new_size + MIN_BLOCK_SIZE)
    {
        return;
    }

    if (lock)
    {
        allocator_lock(allocator);
    }

    size_t block_offset = allocator_block_offset(allocator, block);
    size_t new_block_offset = block_offset + new_size;

    // Create a new block
    kb_block_header_t *new_block = allocator_offset_to_block(allocator, new_block_offset);
    allocator_write_block_tags(allocator, new_block, block->size - new_size, KB_BLOCK_TAG_FREE);
    new_block->next_free_block_offset = block->next_free_block_offset;

    // Update the current block
    allocator_write_block_tags(allocator, block, new_size, KB_BLOCK_TAG_ALLOCATED);

    // Add the new block to the free list
    allocator_add_free_block(allocator, new_block);

    if (lock)
    {
        allocator_unlock(allocator);
    }
}

void allocator_trim(kb_allocator_t *allocator, void *ptr, size_t new_size)
{
    assert(allocator != NULL);
    assert(ptr != NULL);
    assert(new_size > 0);

    size_t total_size = ALIGN(new_size) + BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE;
    kb_block_header_t *block = OFFSET_POINTER(ptr, -BLOCK_HEADER_SIZE);
    allocator_trim_block(allocator, block, total_size, true);
}

void allocator_dump(kb_allocator_t *allocator)
{
#if 1
    return;
#endif

    kb_allocator_header_t *alloc_header = allocator->header;

    log4c_category_info(allocator->logger, "Allocator dump:");
    log4c_category_info(allocator->logger, "  Max message size: %zu", alloc_header->max_message_size);
    log4c_category_info(allocator->logger, "  Total size: %zu", alloc_header->total_size);
    log4c_category_info(allocator->logger, "  Free size: %zu", alloc_header->free_size);
    log4c_category_info(allocator->logger, "  Blocks:");

    // Let's find the first block
    kb_block_header_t *current_block = allocator_offset_to_block(allocator, ALIGN(sizeof(kb_allocator_header_t)));

    while (current_block != NULL)
    {
        log4c_category_info(allocator->logger, "    [%zu: %zu] -> %zu: %c",
                            (char *)current_block - (char *)alloc_header - ALIGN(sizeof(kb_allocator_header_t)),
                            current_block->size,
                            current_block->next_free_block_offset - ALIGN(sizeof(kb_allocator_header_t)),
                            current_block->type == KB_BLOCK_TAG_FREE ? '-' : '+');

        size_t next_block_offset = allocator_block_offset(allocator, current_block) + current_block->size;
        if (next_block_offset >= alloc_header->total_size)
        {
            break;
        }

        current_block = allocator_offset_to_block(allocator, next_block_offset);
    }
}