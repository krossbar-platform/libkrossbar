#if defined(IO_URING_FUTEXES)

#include <gtest/gtest.h>
#include <log4c.h>

extern "C"
{
    #include <shmem/allocator.h>
}

TEST(Allocator, TestAllocatorBlockAdding)
{
    auto logger = log4c_category_get("libkrossbar.test");

    std::array<uint8_t, 1024> memory;
    auto allocator = allocator_create(memory.data(), memory.size(), 128, logger);
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

    allocator_free(allocator, chunk3);
    allocator_dump(allocator);

    allocator_free(allocator, chunk2);
    allocator_dump(allocator);

    allocator_destroy(allocator);
}

#endif