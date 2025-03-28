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
}

#endif