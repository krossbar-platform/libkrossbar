#include <gtest/gtest.h>

extern "C" {
#include <shmem/transport_shm.h>
#include <shmem/message_writer_shm.h>
}

static constexpr size_t ARENA_SIZE = 1024;
static constexpr size_t MESSAGE_SIZE = 1024;

TEST(Transport, TestShmemTransport) {
    auto transport = transport_shm_init("test", ARENA_SIZE, MESSAGE_SIZE);
    auto shm_transport = (kb_transport_shm_t *)transport;
    auto arena = &shm_transport->arena;

    EXPECT_EQ(arena->size, ARENA_SIZE);

    auto message_writer = transport_shm_message_init(transport);
    auto shm_writer = (kb_message_writer_shm_t *)message_writer;
}
