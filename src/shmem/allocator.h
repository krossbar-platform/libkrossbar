#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <log4c/category.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Block type enumeration
 */
enum kb_block_type_e
{
    KB_BLOCK_TAG_FREE = 0x0,
    KB_BLOCK_TAG_ALLOCATED = 0x1
};

typedef enum kb_block_type_e kb_block_type_t;

/**
 * @brief Block header structure preceding each block
 */
struct kb_block_header_s
{
    size_t size;                   // Size of the block (including tags)
    kb_block_type_t type;          // Free or allocated
    size_t next_free_block_offset; // Offset of the next block in the list
};

typedef struct kb_block_header_s kb_block_header_t;

/**
 * @brief Block footer structure following each block.
 *        Used for coalescing adjacent free blocks.
 */
struct kb_block_footer_s
{
    size_t size;          // Size of the block (including tags)
    kb_block_type_t type; // Free or allocated
};

typedef struct kb_block_footer_s kb_block_footer_t;

/**
 * @brief Allocator header structure at the beginning of the shared memory region
 */
struct kb_allocator_header_s
{
    uint32_t futex;                  // Futex for synchronization
    size_t total_size;               // Total size of the shared memory region
    size_t free_size;                // Currently used size
    size_t next_free_block_offset;   // Offset of the first free block
};

typedef struct kb_allocator_header_s kb_allocator_header_t;

/**
 * @brief Allocator structure
 */
struct kb_allocator_s
{
    kb_allocator_header_t *header;   // Header for the shared memory region
    log4c_category_t *logger;        // Logger for debugging
    size_t max_message_size;         // Maximum message size for initial allocations
};

typedef struct kb_allocator_s kb_allocator_t;

/**
 * @brief Create a new allocator in the shared memory region
 *
 * @param memory Shared memory region
 * @param total_size Memory region size
 * @param max_message_size Maximum message size for initial allocations
 * @param logger Logger for debugging
 * @return New allocator instance
 */
kb_allocator_t *allocator_create(void *memory, size_t total_size, size_t max_message_size, log4c_category_t *logger);

/**
 * @brief Destroy the allocator and free all resources
 *
 * @param allocator
 */
void allocator_destroy(kb_allocator_t *allocator);

/**
 * Allocate a block for a message
 * Always allocates the maximum message size initially
 *
 * @param allocator Pointer to the allocator
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void *allocator_alloc(kb_allocator_t *allocator);

/**
 * Free a block of memory
 *
 * @param allocator Pointer to the allocator
 * @param ptr Pointer to the memory block to free
 */
void allocator_free(kb_allocator_t *allocator, void *ptr);

/**
 * @brief Lock the allocator for free list operations
 * 
 * @param allocator 
 */
void allocator_lock(kb_allocator_t *allocator);

/**
 * @brief Unlock the allocator after free list operations
 * 
 * @param allocator 
 */
void allocator_unlock(kb_allocator_t *allocator);

/**
 * @brief Get the offset of a block in the shared memory region
 * 
 * @param allocator Memory allocator
 * @param block Block header
 * @return Offset of the block
 */
size_t allocator_block_offset(kb_allocator_t *allocator, kb_block_header_t *block);

/**
 * @brief Get the block header from an offset
 * 
 * @param allocator Memory allocator
 * @param offset Block offset
 * @return A pointer to the block header
 */
kb_block_header_t *allocator_offset_to_block(kb_allocator_t *allocator, size_t offset);

/**
 * @brief Write block header and footer tags
 * 
 * @param allocator Memory allocator
 * @param block A block
 * @param size Block size
 * @param type Block type (free or allocated)
 */
void allocator_write_block_tags(kb_allocator_t *allocator, kb_block_header_t *block,
                                size_t size, kb_block_type_t type);

/**
 * @brief Coallesce adjacent free blocks
 * 
 * @param allocator Memory allocator
 * @param block A block
 */
void allocator_coalesce_free_blocks(kb_allocator_t *allocator, kb_block_header_t *block);

/**
 * @brief Trim an allocated block to a new size
 * 
 * @param allocator Memory allocator
 * @param block Allocated block
 * @param new_size New size for the block
 * @param lock Lock the allocator for the operation
 */
void allocator_trim_block(kb_allocator_t *allocator, kb_block_header_t *block, size_t actual_size, bool lock);

/**
 * @brief Dump the allocator state for debugging
 * 
 * @param allocator Memory allocator
 */
void allocator_dump(kb_allocator_t *allocator);

#ifdef __cplusplus
} // extern "C"
#endif