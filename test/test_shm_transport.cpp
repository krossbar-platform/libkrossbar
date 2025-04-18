#if defined(IO_URING_FUTEXES)

#include <string>

#include <gtest/gtest.h>
#include <liburing.h>
#include <log4c.h>

#include <shmem/transport_shm.h>
#include <shmem/message_shm.h>
#include <shmem/message_writer_shm.h>

static constexpr size_t ARENA_SIZE = 768;
static constexpr size_t MESSAGE_SIZE = 128;
static constexpr size_t RING_QUEUE_DEPTH = 32;

static constexpr size_t BUFFER_SIZE = MESSAGE_SIZE - 2;
static char RANDOM_BUFFER[BUFFER_SIZE] = {};

TEST(Transport, TestShmemTransport) {
    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    auto logger = log4c_category_get("libkrossbar.test");
    auto map_fd_0 = transport_shm_create_mapping("map0", ARENA_SIZE, logger);
    auto map_fd_1 = transport_shm_create_mapping("map1", ARENA_SIZE, logger);

    auto transport_writer = transport_shm_init("test_writer", map_fd_0, map_fd_1, MESSAGE_SIZE, &ring, logger);
    auto shm_transport_writer = (kb_transport_shm_t *)transport_writer;
    auto arena = &shm_transport_writer->write_arena;

    ASSERT_EQ(arena->header->size, ARENA_SIZE);

    auto message_writer = transport_message_init(transport_writer);
    auto shm_writer = (kb_message_writer_shm_t *)message_writer;

    ASSERT_EQ(shm_writer->header->size, MESSAGE_SIZE);
    message_write_bool(message_writer, true);
    message_write_u64(message_writer, 42);
    message_write_cstr(message_writer, "Hello world!");

    message_start_array(message_writer, 3);
    message_write_u64(message_writer, 1);
    message_write_u64(message_writer, 2);
    message_write_u64(message_writer, 3);
    message_finish_array(message_writer);

    message_build_map(message_writer);
    message_write_cstr(message_writer, "one");
    message_write_i8(message_writer, 1);
    message_write_cstr(message_writer, "two");
    message_write_i8(message_writer, 2);
    message_write_cstr(message_writer, "three");
    message_write_i8(message_writer, 3);
    message_complete_map(message_writer);

    message_send(message_writer);

    ASSERT_EQ(arena->header->num_messages, 1);

    auto transport_reader = transport_shm_init("test_reader", map_fd_1, map_fd_0, MESSAGE_SIZE, &ring, logger);
    auto message = transport_message_receive(transport_reader);

    ASSERT_NE(message, nullptr);

    auto shm_message = (kb_message_shm_t *)message;

    ASSERT_EQ(shm_message->header->size, 37);

    auto tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_bool);
    ASSERT_EQ(tag.v.b, true);

    tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_uint);
    ASSERT_EQ(tag.v.u, 42);

    tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_str);
    const char *data = message_read_bytes(message, tag.v.l);
    std::string str{data, tag.v.l};
    ASSERT_STREQ(str.c_str(), "Hello world!");

    tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_array);
    ASSERT_EQ(tag.v.n, 3);

    for (int i = 1; i < 4; i++)
    {
        tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_uint);
        ASSERT_EQ(tag.v.u, i);
    }

    tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_map);
    ASSERT_EQ(tag.v.n, 3);

    {
        tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_str);
        data = message_read_bytes(message, tag.v.l);
        str = std::string{data, tag.v.l};
        ASSERT_STREQ(str.c_str(), "one");

        tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_uint);
        ASSERT_EQ(tag.v.u, 1);

        tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_str);
        data = message_read_bytes(message, tag.v.l);
        str = std::string{data, tag.v.l};
        ASSERT_STREQ(str.c_str(), "two");

        tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_uint);
        ASSERT_EQ(tag.v.u, 2);

        tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_str);
        data = message_read_bytes(message, tag.v.l);
        str = std::string{data, tag.v.l};
        ASSERT_STREQ(str.c_str(), "three");

        tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_uint);
        ASSERT_EQ(tag.v.u, 3);
    }

    message_destroy(message);
    ASSERT_EQ(arena->header->num_messages, 0);

    ASSERT_EQ(transport_message_receive(transport_writer), nullptr);

    transport_destroy(transport_writer);
    transport_destroy(transport_reader);
}

TEST(Transport, TestShmemCycle)
{
    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    auto logger = log4c_category_get("libkrossbar.test");
    auto map_fd_0 = transport_shm_create_mapping("map0", ARENA_SIZE, logger);
    auto map_fd_1 = transport_shm_create_mapping("map1", ARENA_SIZE, logger);

    auto transport_writer = (kb_transport_shm_t *)transport_shm_init("test", map_fd_0, map_fd_1, MESSAGE_SIZE, &ring, logger);
    auto arena = &transport_writer->write_arena;

    auto message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);
    ASSERT_EQ(arena->header->num_messages, 1);

    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);
    ASSERT_EQ(arena->header->num_messages, 2);

    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);
    ASSERT_EQ(arena->header->num_messages, 3);

    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_EQ(message_writer, nullptr);

    auto transport_reader = transport_shm_init("test", map_fd_1, map_fd_0, MESSAGE_SIZE, &ring, logger);
    auto message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);

    auto tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_bin);
    ASSERT_EQ(tag.v.l, BUFFER_SIZE);

    ASSERT_EQ(arena->header->num_messages, 2);
    message_destroy(message);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);
    ASSERT_EQ(arena->header->num_messages, 1);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);
    ASSERT_EQ(arena->header->num_messages, 0);

    message = transport_message_receive(transport_reader);
    ASSERT_EQ(message, nullptr);

    transport_destroy(&transport_writer->base);
    transport_destroy(transport_reader);
}

TEST(Transport, TestShmemReplace)
{
    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    auto logger = log4c_category_get("libkrossbar.test");
    auto map_fd_0 = transport_shm_create_mapping("map0", ARENA_SIZE, logger);
    auto map_fd_1 = transport_shm_create_mapping("map1", ARENA_SIZE, logger);

    auto transport_writer = (kb_transport_shm_t *)transport_shm_init("test", map_fd_0, map_fd_1, MESSAGE_SIZE, &ring, logger);

    auto transport_reader = transport_shm_init("test", map_fd_1, map_fd_0, MESSAGE_SIZE, &ring, logger);

    auto message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    // [1, 1, 1]
    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_EQ(message_writer, nullptr);

    auto message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    // [0, 1, 1]
    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    // [1, 1, 1]
    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_EQ(message_writer, nullptr);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    // [0, 0, 1]
    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    // [1, 1, 1]
    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_EQ(message_writer, nullptr);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    // [0, 0, 0]
    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    // [1, 1, 1]
    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_EQ(message_writer, nullptr);

    transport_destroy(&transport_writer->base);
    transport_destroy(transport_reader);
}

TEST(Transport, TestShmemSingleReplace)
{
    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    auto logger = log4c_category_get("libkrossbar.test");
    auto map_fd_0 = transport_shm_create_mapping("map0", ARENA_SIZE, logger);
    auto map_fd_1 = transport_shm_create_mapping("map1", ARENA_SIZE, logger);

    auto transport_writer = (kb_transport_shm_t *)transport_shm_init("test", map_fd_0, map_fd_1, MESSAGE_SIZE, &ring, logger);
    auto arena = &transport_writer->write_arena;

    auto transport_reader = transport_shm_init("test", map_fd_1, map_fd_0, MESSAGE_SIZE, &ring, logger);

    auto message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_NE(message_writer, nullptr);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    // [1, 1, 1]
    message_writer = transport_message_init(&transport_writer->base);
    ASSERT_EQ(message_writer, nullptr);

    auto message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    // [0, 1, 1]
    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    // [1, 0, 1]
    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);

    // [1, 1, 0]
    message_writer = transport_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    // [1, 1, 1]
    ASSERT_EQ(arena->header->num_messages, 3);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);
    ASSERT_EQ(arena->header->num_messages, 2);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);
    ASSERT_EQ(arena->header->num_messages, 1);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    message_destroy(message);
    ASSERT_EQ(arena->header->num_messages, 0);

    message = transport_message_receive(transport_reader);
    ASSERT_EQ(message, nullptr);

    transport_destroy(&transport_writer->base);
    transport_destroy(transport_reader);
}

TEST(Transport, TestShmemMessageCancel)
{
    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    auto logger = log4c_category_get("libkrossbar.test");
    auto map_fd_0 = transport_shm_create_mapping("map0", ARENA_SIZE, logger);
    auto map_fd_1 = transport_shm_create_mapping("map1", ARENA_SIZE, logger);

    auto transport_writer = transport_shm_init("test", map_fd_0, map_fd_1, MESSAGE_SIZE, &ring, logger);
    auto shm_transport_writer = (kb_transport_shm_t *)transport_writer;
    auto arena = &shm_transport_writer->write_arena;

    ASSERT_EQ(arena->header->size, ARENA_SIZE);

    auto message_writer = transport_message_init(transport_writer);
    message_cancel(message_writer);

    ASSERT_EQ(arena->header->num_messages, 0);

    transport_destroy(transport_writer);
}

#endif