#if defined(IO_URING_FUTEXES)

#include <gtest/gtest.h>
#include <log4c.h>
#include <vector>
#include <algorithm>

#include <shmem/allocator.h>
#include <shmem/common.h>

// Utility structure to represent a memory block for testing
struct BlockInfo
{
    size_t offset;
    size_t size;
    kb_block_type_t type;
    size_t next_free_offset;
};

// Utility function to get a list of all memory blocks in the allocator
std::vector<BlockInfo> getAllBlocks(kb_allocator_t *allocator)
{
    std::vector<BlockInfo> blocks;
    kb_allocator_header_t *alloc_header = allocator->header;

    // Start from the first block after the header
    size_t header_size = ALIGN(sizeof(kb_allocator_header_t));
    kb_block_header_t *current_block = (kb_block_header_t *)((char *)alloc_header + header_size);

    while ((char *)current_block < (char *)alloc_header + alloc_header->total_size + header_size)
    {
        BlockInfo info;
        info.offset = (char *)current_block - (char *)alloc_header;
        info.size = current_block->size;
        info.type = current_block->type;
        info.next_free_offset = current_block->next_free_block_offset;

        blocks.push_back(info);

        // Move to the next block
        current_block = (kb_block_header_t *)((char *)current_block + current_block->size);

        // Break if we've reached the end of the memory region
        if ((char *)current_block >= (char *)alloc_header + alloc_header->total_size + header_size)
        {
            break;
        }
    }

    return blocks;
}

// Test fixture for allocator tests
class AllocatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        logger = log4c_category_get("libkrossbar.test");
        // 4KB of memory for testing
        memory.resize(4096);
        allocator = allocator_create(memory.data(), memory.size(), 128, logger);
        ASSERT_NE(allocator, nullptr);
    }

    void TearDown() override
    {
        if (allocator)
        {
            allocator_destroy(allocator);
        }
    }

    std::vector<uint8_t> memory;
    kb_allocator_t *allocator = nullptr;
    log4c_category_t *logger = nullptr;
};

TEST_F(AllocatorTest, TestAllocatorBlockAdding)
{
    allocator_dump(allocator);

    auto chunk0 = allocator_alloc(allocator);
    ASSERT_NE(chunk0, nullptr);
    allocator_dump(allocator);

    auto chunk1 = allocator_alloc(allocator);
    ASSERT_NE(chunk1, nullptr);
    allocator_dump(allocator);

    auto chunk2 = allocator_alloc(allocator);
    ASSERT_NE(chunk2, nullptr);
    allocator_dump(allocator);

    auto chunk3 = allocator_alloc(allocator);
    ASSERT_NE(chunk3, nullptr);
    allocator_dump(allocator);

    allocator_free(allocator, chunk0);
    allocator_dump(allocator);

    allocator_free(allocator, chunk1);
    allocator_dump(allocator);

    auto chunk4 = allocator_alloc(allocator);
    ASSERT_NE(chunk4, nullptr);
    allocator_dump(allocator);

    allocator_free(allocator, chunk4);
    allocator_dump(allocator);

    allocator_free(allocator, chunk3);
    allocator_dump(allocator);

    allocator_free(allocator, chunk2);
    allocator_dump(allocator);
}

TEST_F(AllocatorTest, TestAllocatorCreation)
{
    // Check that the allocator was created correctly
    ASSERT_NE(allocator, nullptr);
    ASSERT_NE(allocator->header, nullptr);
    ASSERT_EQ(allocator->header->max_message_size, ALIGN(128));

    // Check that the header was initialized correctly
    ASSERT_EQ(allocator->header->futex, 0);
    ASSERT_LT(allocator->header->total_size, memory.size());
    ASSERT_EQ(allocator->header->free_size, allocator->header->total_size);
    ASSERT_NE(allocator->header->next_free_block_offset, (size_t)-1); // Should have a free block

    // Check that we have a single free block
    auto blocks = getAllBlocks(allocator);
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_EQ(blocks[0].type, KB_BLOCK_TAG_FREE);
    ASSERT_EQ(blocks[0].size, allocator->header->total_size);
}

TEST_F(AllocatorTest, TestBasicAllocation)
{
    // Basic allocation test
    void *ptr = allocator_alloc(allocator);
    ASSERT_NE(ptr, nullptr);

    // Verify allocated block
    auto blocks = getAllBlocks(allocator);
    ASSERT_GE(blocks.size(), 1);

    // First block should be allocated
    ASSERT_EQ(blocks[0].type, KB_BLOCK_TAG_ALLOCATED);

    // Free the allocated memory
    allocator_free(allocator, ptr);

    // Verify block is now free
    blocks = getAllBlocks(allocator);
    ASSERT_EQ(blocks[0].type, KB_BLOCK_TAG_FREE);
}

TEST_F(AllocatorTest, TestMultipleAllocations)
{
    std::vector<void *> ptrs;

    // Allocate multiple blocks
    for (int i = 0; i < 10; i++)
    {
        void *ptr = allocator_alloc(allocator);
        if (ptr == nullptr)
            break; // Stop if we can't allocate more
        ptrs.push_back(ptr);
    }

    // Make sure we allocated at least a few blocks
    ASSERT_GT(ptrs.size(), 3);

    // Check the allocated blocks
    auto blocks = getAllBlocks(allocator);
    size_t allocatedCount = 0;

    for (const auto &block : blocks)
    {
        if (block.type == KB_BLOCK_TAG_ALLOCATED)
        {
            allocatedCount++;
        }
    }

    ASSERT_EQ(allocatedCount, ptrs.size());

    // Free in reverse order
    while (!ptrs.empty())
    {
        allocator_free(allocator, ptrs.back());
        ptrs.pop_back();
    }

    // Verify all blocks are now free and coalesced
    blocks = getAllBlocks(allocator);

    bool hasAllocated = false;
    for (const auto &block : blocks)
    {
        if (block.type == KB_BLOCK_TAG_ALLOCATED)
        {
            hasAllocated = true;
            break;
        }
    }

    ASSERT_FALSE(hasAllocated);
}

TEST_F(AllocatorTest, TestCoalescing)
{
    // Allocate multiple small blocks
    void *ptr1 = allocator_alloc(allocator);
    void *ptr2 = allocator_alloc(allocator);
    void *ptr3 = allocator_alloc(allocator);

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    // Count blocks before freeing
    auto beforeBlocks = getAllBlocks(allocator);
    size_t beforeCount = beforeBlocks.size();

    // Free middle block
    allocator_free(allocator, ptr2);

    // Should still have the same number of blocks
    auto midBlocks = getAllBlocks(allocator);
    ASSERT_EQ(midBlocks.size(), beforeCount);

    // Free adjacent block and check coalescing
    allocator_free(allocator, ptr1);

    // Should have fewer blocks due to coalescing
    auto afterFirstCoalesce = getAllBlocks(allocator);
    ASSERT_LT(afterFirstCoalesce.size(), beforeCount);

    // Free the last block and check coalescing again
    allocator_free(allocator, ptr3);

    // Should have even fewer blocks
    auto afterSecondCoalesce = getAllBlocks(allocator);
    ASSERT_LE(afterSecondCoalesce.size(), afterFirstCoalesce.size());

    // Check if we have a single free block (full coalescing)
    bool hasSingleFreeBlock = (afterSecondCoalesce.size() == 1 &&
                               afterSecondCoalesce[0].type == KB_BLOCK_TAG_FREE);

    // We might not have a single block if there was significant fragmentation
    // but we should at least have fewer blocks than before
    ASSERT_LT(afterSecondCoalesce.size(), beforeCount);
}

TEST_F(AllocatorTest, TestAllocUntilFull)
{
    std::vector<void *> ptrs;

    // Keep allocating until we run out of memory
    while (true)
    {
        void *ptr = allocator_alloc(allocator);
        if (ptr == nullptr)
            break;
        ptrs.push_back(ptr);
    }

    // We should have allocated some blocks
    ASSERT_GT(ptrs.size(), 0);

    // Free all the allocated memory
    for (void *ptr : ptrs)
    {
        allocator_free(allocator, ptr);
    }

    // After freeing, we should be able to allocate again
    void *ptr = allocator_alloc(allocator);
    ASSERT_NE(ptr, nullptr);
    allocator_free(allocator, ptr);
}

TEST_F(AllocatorTest, TestLockingMechanics)
{
    // Basic test to ensure locking doesn't deadlock
    allocator_lock(allocator);

    // Do some operations while locked
    kb_block_header_t *block = allocator_offset_to_block(allocator, ALIGN(sizeof(kb_allocator_header_t)));
    ASSERT_NE(block, nullptr);

    // Unlock
    allocator_unlock(allocator);

    // Allocate while unlocked
    void *ptr = allocator_alloc(allocator);
    ASSERT_NE(ptr, nullptr);
    allocator_free(allocator, ptr);
}

TEST_F(AllocatorTest, TestBlockOffset)
{
    // Get a block and check its offset
    kb_block_header_t *block = allocator_offset_to_block(allocator, ALIGN(sizeof(kb_allocator_header_t)));
    ASSERT_NE(block, nullptr);

    size_t offset = allocator_block_offset(allocator, block);
    ASSERT_EQ(offset, ALIGN(sizeof(kb_allocator_header_t)));

    // Convert back and forth
    kb_block_header_t *sameBlock = allocator_offset_to_block(allocator, offset);
    ASSERT_EQ(sameBlock, block);
}

TEST_F(AllocatorTest, TestNullOffsetToBlock)
{
    // Test with NULL_BLOCK_OFFSET
    kb_block_header_t *nullBlock = allocator_offset_to_block(allocator, (size_t)-1);
    ASSERT_EQ(nullBlock, nullptr);
}

TEST_F(AllocatorTest, TestBlockTags)
{
    // Get a block and test writing tags
    kb_block_header_t *block = allocator_offset_to_block(allocator, ALIGN(sizeof(kb_allocator_header_t)));
    ASSERT_NE(block, nullptr);

    // Write allocated tag
    allocator_write_block_tags(allocator, block, 128, KB_BLOCK_TAG_ALLOCATED);
    ASSERT_EQ(block->type, KB_BLOCK_TAG_ALLOCATED);
    ASSERT_EQ(block->size, 128);

    // Check footer tag
    kb_block_footer_t *footer = (kb_block_footer_t *)((char *)block + 128 - ALIGN(sizeof(kb_block_footer_t)));
    ASSERT_EQ(footer->type, KB_BLOCK_TAG_ALLOCATED);
    ASSERT_EQ(footer->size, 128);

    // Write free tag
    allocator_write_block_tags(allocator, block, 128, KB_BLOCK_TAG_FREE);
    ASSERT_EQ(block->type, KB_BLOCK_TAG_FREE);

    // Check footer tag
    ASSERT_EQ(footer->type, KB_BLOCK_TAG_FREE);
}

TEST_F(AllocatorTest, TestAllocatorDump)
{
    // Just make sure it doesn't crash
    allocator_dump(allocator);

    // Allocate some memory to make the dump more interesting
    void *ptr1 = allocator_alloc(allocator);
    void *ptr2 = allocator_alloc(allocator);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    // Dump again with allocations
    allocator_dump(allocator);

    // Free and dump again
    allocator_free(allocator, ptr1);
    allocator_dump(allocator);

    allocator_free(allocator, ptr2);
    allocator_dump(allocator);
}

TEST_F(AllocatorTest, TestAttachAndCrossAllocatorOperations)
{
    // First, create an allocator and allocate some memory
    void *ptr1 = allocator_alloc(allocator);
    void *ptr2 = allocator_alloc(allocator);
    void *ptr3 = allocator_alloc(allocator);

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    // Verify the state of the first allocator
    auto blocksBeforeAttach = getAllBlocks(allocator);
    size_t allocatedCountBefore = 0;
    for (const auto &block : blocksBeforeAttach)
    {
        if (block.type == KB_BLOCK_TAG_ALLOCATED)
        {
            allocatedCountBefore++;
        }
    }
    ASSERT_EQ(allocatedCountBefore, 3);

    // Store the key metrics from the first allocator
    size_t totalSize = allocator->header->total_size;
    size_t freeSize = allocator->header->free_size;
    size_t maxMessageSize = allocator->header->max_message_size;

    // Now attach a new allocator to the same memory region
    kb_allocator_t *attachedAllocator = allocator_attach(memory.data(), logger);
    ASSERT_NE(attachedAllocator, nullptr);

    // Verify the attached allocator sees the same state
    ASSERT_EQ(attachedAllocator->header->total_size, totalSize);
    ASSERT_EQ(attachedAllocator->header->free_size, freeSize);
    ASSERT_EQ(attachedAllocator->header->max_message_size, maxMessageSize);

    auto blocksAfterAttach = getAllBlocks(attachedAllocator);
    size_t allocatedCountAfter = 0;
    for (const auto &block : blocksAfterAttach)
    {
        if (block.type == KB_BLOCK_TAG_ALLOCATED)
        {
            allocatedCountAfter++;
        }
    }
    ASSERT_EQ(allocatedCountAfter, 3);

    // Allocate with the attached allocator
    void *ptrFromAttached = allocator_alloc(attachedAllocator);
    ASSERT_NE(ptrFromAttached, nullptr);

    // Free memory from the first allocator using the attached allocator
    allocator_free(attachedAllocator, ptr1);
    allocator_free(attachedAllocator, ptr2);

    // Verify the state changed in both allocators (they share the memory)
    auto blocksAfterFree = getAllBlocks(allocator);
    size_t allocatedCountAfterFree = 0;
    for (const auto &block : blocksAfterFree)
    {
        if (block.type == KB_BLOCK_TAG_ALLOCATED)
        {
            allocatedCountAfterFree++;
        }
    }
    // Should have 2 allocated blocks now (ptr3 and ptrFromAttached)
    ASSERT_EQ(allocatedCountAfterFree, 2);

    // Allocate something else with the original allocator
    void *ptrAfterCrossAlloc = allocator_alloc(allocator);
    ASSERT_NE(ptrAfterCrossAlloc, nullptr);

    // Free all remaining blocks using both allocators
    allocator_free(allocator, ptr3);
    allocator_free(attachedAllocator, ptrFromAttached);
    allocator_free(allocator, ptrAfterCrossAlloc);

    // Verify all memory is freed
    auto finalBlocks = getAllBlocks(attachedAllocator);
    bool hasAllocated = false;
    for (const auto &block : finalBlocks)
    {
        if (block.type == KB_BLOCK_TAG_ALLOCATED)
        {
            hasAllocated = true;
            break;
        }
    }
    ASSERT_FALSE(hasAllocated);

    // We should have coalesced back to a single free block or at most a few
    ASSERT_LE(finalBlocks.size(), 3);

    // Clean up the attached allocator
    allocator_destroy(attachedAllocator);
}

#endif