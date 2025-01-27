#include <string>

#include <gtest/gtest.h>

extern "C" {
#include <shmem/transport_shm.h>
#include <shmem/message_shm.h>
#include <shmem/message_writer_shm.h>
}

static constexpr size_t ARENA_SIZE = 512;
static constexpr size_t MESSAGE_SIZE = 128;

static constexpr size_t BUFFER_SIZE = MESSAGE_SIZE - 2;
static char RANDOM_BUFFER[BUFFER_SIZE] = {};

TEST(Transport, TestShmemTransport) {
    auto transport_writer = transport_shm_init("test", ARENA_SIZE, MESSAGE_SIZE);
    auto shm_transport_writer = (kb_transport_shm_t *)transport_writer;
    auto arena = &shm_transport_writer->arena;

    ASSERT_EQ(arena->header->size, ARENA_SIZE);

    auto message_writer = transport_shm_message_init(transport_writer);
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

    ASSERT_EQ(arena->header->write_offset, 37 + sizeof(kb_message_header_t));
    ASSERT_EQ(arena->header->read_offset, 0);

    auto transport_reader = transport_shm_connect("test_reader", transport_shm_get_fd(transport_writer));
    auto message = transport_shm_message_receive(transport_reader);

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

    ASSERT_EQ(transport_shm_message_receive(transport_writer), nullptr);

    ASSERT_EQ(arena->header->read_offset, 0);
    ASSERT_EQ(arena->header->write_offset, 0);

    transport_shm_destroy(transport_writer);
}

TEST(Transport, TestShmemMemory)
{
    auto transport_writer = (kb_transport_shm_t *)transport_shm_init("test", ARENA_SIZE, MESSAGE_SIZE);
    auto arena = &transport_writer->arena;

    ASSERT_EQ(arena->header->write_offset, 0);

    auto message_writer = transport_shm_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);
    ASSERT_EQ(arena->header->write_offset, 144);

    message_writer = transport_shm_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);
    ASSERT_EQ(arena->header->write_offset, 288);

    message_writer = transport_shm_message_init(&transport_writer->base);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);
    ASSERT_EQ(arena->header->write_offset, 432);

    message_writer = transport_shm_message_init(&transport_writer->base);
    ASSERT_EQ(message_writer, nullptr);

    auto transport_reader = transport_shm_connect("test_reader", transport_writer->shm_fd);
    auto message = transport_shm_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);

    auto tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_bin);
    ASSERT_EQ(tag.v.l, BUFFER_SIZE);
    transport_shm_message_release(transport_reader, message);

    message = transport_shm_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    transport_shm_message_release(transport_reader, message);

    message = transport_shm_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    transport_shm_message_release(transport_reader, message);

    message = transport_shm_message_receive(transport_reader);
    ASSERT_EQ(message, nullptr);
}