#include "allocator.h"

#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

// Alignment for all allocations (64-bit)
#define ALIGNMENT 8
// Round up to nearest multiple of ALIGNMENT
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// No block offset (used for head/tail pointers)
#define NULL_BLOCK_OFFSET ((size_t)-1)

// Block header size
#define BLOCK_HEADER_SIZE ALIGN(sizeof(kb_block_header_t))
// Minimum block size (including header)
#define MIN_BLOCK_SIZE (BLOCK_HEADER_SIZE + 64)

// Futex utility functions
static int futex_wait(uint32_t *uaddr, int val)
{
    return syscall(SYS_futex, uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
}

static int futex_wake(uint32_t *uaddr, int count)
{
    return syscall(SYS_futex, uaddr, FUTEX_WAKE, count, NULL, NULL, 0);
}

kb_allocator_t *allocator_create(void *memory, size_t total_size, size_t max_message_size, log4c_category_t *logger)
{

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
    allocator->max_message_size = ALIGN(max_message_size);

    size_t header_size = ALIGN(sizeof(kb_allocator_header_t));

    // Initialize the allocator header
    alloc_header->futex = 0;
    alloc_header->total_size = total_size - header_size;
    alloc_header->used_size = header_size;
    alloc_header->free_blocks.head_offset = NULL_BLOCK_OFFSET;
    alloc_header->free_blocks.tail_offset = header_size;
    alloc_header->blocks.head_offset = NULL_BLOCK_OFFSET;
    alloc_header->blocks.tail_offset = header_size;

    // Initialize the first block
    kb_block_header_t *first_block = (kb_block_header_t*)(memory + header_size);
    first_block->size = alloc_header->total_size - alloc_header->used_size;
    first_block->is_free = true;
    first_block->prev_offset = 0;
    first_block->next_offset = 0;

    // Add the block to the free list
    allocator_add_free_block(allocator, first_block);

    return allocator;
}


void allocator_lock(kb_allocator_t *allocator)
{
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

void allocator_unlock(kb_allocator_t *allocator)
{
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

void *allocator_list_head(kb_allocator_t *allocator, kb_allocator_list_t *list)
{
    if (list->head_offset == NULL_BLOCK_OFFSET)
    {
        return NULL;
    }

    return (char*)allocator->header + list->head_offset;
}

void *allocator_list_tail(kb_allocator_t *allocator, kb_allocator_list_t *list)
{
    if (list->tail_offset == NULL_BLOCK_OFFSET)
    {
        return NULL;
    }

    return (char*)allocator->header + list->tail_offset;
}

inline size_t allocator_block_offset(kb_allocator_t *allocator, kb_block_header_t *block)
{
    return (char *)block - (char *)allocator->header;
}

kb_block_header_t *allocator_get_block(kb_allocator_t *allocator, size_t offset)
{
    return (kb_block_header_t *)((char *)allocator->header + offset);
}

void *allocator_alloc(kb_allocator_t *allocator)
{
    // Always allocate the maximum message size initially
    size_t alloc_size = allocator->max_message_size + BLOCK_HEADER_SIZE;

    allocator_lock(allocator);

    // Find a suitable free block (best fit strategy)
    kb_block_header_t *block = allocator_get_block(allocator, allocator->header->free_blocks.head_offset);
    kb_block_header_t *best_fit = NULL;

    // Find the best fit block
    while (block)
    {
        if (block->size >= alloc_size)
        {
            best_fit = block;
            break;
        }

        block = allocator_get_block(allocator, block->next_offset);
    }

    if (!best_fit)
    {
        // No suitable block found
        allocator_unlock(allocator);
        return NULL;
    }

    // Remove the block from the free list
    allocator_remove_free_block(allocator, best_fit);
    size_t best_offset = allocator_block_offset(allocator, best_fit);

    // Split the block if it's too large
    if (best_fit->size >= alloc_size + MIN_BLOCK_SIZE)
    {
        size_t new_block_offset = best_offset + alloc_size;

        // Create a new block
        kb_block_header_t *new_block = allocator_get_block(allocator, new_block_offset);
        new_block->size = best_fit->size - alloc_size;
        new_block->is_free = true;
        new_block->next_offset = best_fit->next_offset;
        new_block->prev_offset = best_offset;

        // Update the next block's previous pointer if applicable
        if (best_fit->next_offset != NULL_BLOCK_OFFSET)
        {
            kb_block_header_t *next_block = allocator_get_block(allocator, best_fit->next_offset);
            next_block->prev_offset = allocator_block_offset(allocator, new_block);
        }
        // Or update the tail pointer if the block is the tail
        else
        {
            allocator->header->free_blocks.tail_offset = allocator_block_offset(allocator, new_block);
        }

        // Update the best block
        best_fit->next_offset = new_block_offset;
        best_fit->size = alloc_size;

        // Add the new block to the free list
        allocator_add_free_block(allocator, new_block);
    }

    // Mark the block as allocated
    best_fit->is_free = false;
    allocator->header->used_size += best_fit->size;

    allocator_unlock(allocator);

    return best_fit + BLOCK_HEADER_SIZE;
}

void allocator_add_free_block(kb_allocator_t *allocator, kb_block_header_t *block)
{
    block->is_free = true;
    kb_allocator_header_t *alloc_header = allocator->header;

    size_t block_offset = allocator_block_offset(allocator, block);

    if (alloc_header->free_blocks.head_offset == NULL_BLOCK_OFFSET)
    {
        // List is empty
        alloc_header->free_blocks.head_offset = block_offset;
        alloc_header->free_blocks.tail_offset = block_offset;
        block->next_offset = NULL_BLOCK_OFFSET;
        block->prev_offset = NULL_BLOCK_OFFSET;
    }
    else
    {
        // Add to the end of the list
        // Set the next pointer of the current tail to the new block
        kb_block_header_t *free_list_tail = (kb_block_header_t*)allocator_list_tail(allocator, &alloc_header->free_blocks);
        free_list_tail->next_offset = block_offset;

        // Update the new block previos pointer to the current tail
        block->prev_offset = alloc_header->free_blocks.tail_offset;
        block->next_offset = NULL_BLOCK_OFFSET;

        // Update the tail pointer
        alloc_header->free_blocks.tail_offset = block_offset;
    }
}

void allocator_remove_free_block(kb_allocator_t *allocator, kb_block_header_t *block)
{
    kb_allocator_header_t *alloc_header = allocator->header;

    size_t block_offset = allocator_block_offset(allocator, block);

    if (block->prev_offset == NULL_BLOCK_OFFSET)
    {
        // Block is the head of the list
        alloc_header->free_blocks.head_offset = block->next_offset;
    }
    else
    {
        // Update the previous block's next pointer
        kb_block_header_t *prev_block = allocator_get_block(allocator, block->prev_offset);
        prev_block->next_offset = block->next_offset;
    }

    if (block->next_offset == NULL_BLOCK_OFFSET)
    {
        // Block is the tail of the list
        alloc_header->free_blocks.tail_offset = block->prev_offset;
    }
    else
    {
        // Update the next block's previous pointer
        kb_block_header_t *next_block = allocator_get_block(allocator, block->next_offset);
        next_block->prev_offset = block->prev_offset;
    }
}

void allocator_dump(kb_allocator_t *allocator)
{
    kb_allocator_header_t *alloc_header = allocator->header;

    log4c_category_info(allocator->logger, "Allocator dump:");
    log4c_category_info(allocator->logger, "  Max message size: %zu", allocator->max_message_size);
    log4c_category_info(allocator->logger, "  Total size: %zu", alloc_header->total_size);
    log4c_category_info(allocator->logger, "  Used size: %zu", alloc_header->used_size);
    log4c_category_info(allocator->logger, "  Free blocks:");
    kb_block_header_t *current_block = (kb_block_header_t*)allocator_list_head(allocator, &alloc_header->free_blocks);
    while (current_block != NULL)
    {
        log4c_category_info(allocator->logger, "    Block offset: %zu, size: %zu", (char*)current_block - (char*)alloc_header, current_block->size);

        if (current_block->next_offset == NULL_BLOCK_OFFSET)
        {
            break;
        }

        current_block = (kb_block_header_t*)((char*)&current_block + current_block->next_offset);
    }
}