#include <string>

#include <gtest/gtest.h>

extern "C" {
#include <shmem/transport_shm.h>
#include <shmem/message_shm.h>
#include <shmem/message_writer_shm.h>
}

static constexpr size_t ARENA_SIZE = 1024;
static constexpr size_t MESSAGE_SIZE = 128;

TEST(Transport, TestShmemTransport) {
    auto transport = transport_shm_init("test", ARENA_SIZE, MESSAGE_SIZE);
    auto shm_transport = (kb_transport_shm_t *)transport;
    auto arena = &shm_transport->arena;

    EXPECT_EQ(arena->size, ARENA_SIZE);

    auto message_writer = transport_shm_message_init(transport);
    auto shm_writer = (kb_message_writer_shm_t *)message_writer;

    EXPECT_EQ(shm_writer->header->size, MESSAGE_SIZE);
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

    EXPECT_EQ(arena->header->write_offset, 37 + sizeof(kb_message_header_t));
    EXPECT_EQ(arena->header->read_offset, 0);

    auto message = transport_shm_message_receive(transport);
    auto shm_message = (kb_message_shm_t *)message;

    EXPECT_EQ(shm_message->header->size, 37);

    auto tag = message_read_tag(message);
    EXPECT_EQ(tag.type, mpack_type_bool);
    EXPECT_EQ(tag.v.b, true);

    tag = message_read_tag(message);
    EXPECT_EQ(tag.type, mpack_type_uint);
    EXPECT_EQ(tag.v.u, 42);

    tag = message_read_tag(message);
    EXPECT_EQ(tag.type, mpack_type_str);
    const char *data = message_read_bytes(message, tag.v.l);
    std::string str{data, tag.v.l};
    EXPECT_STREQ(str.c_str(), "Hello world!");

    tag = message_read_tag(message);
    EXPECT_EQ(tag.type, mpack_type_array);
    EXPECT_EQ(tag.v.n, 3);

    for (int i = 1; i < 4; i++)
    {
        tag = message_read_tag(message);
        EXPECT_EQ(tag.type, mpack_type_uint);
        EXPECT_EQ(tag.v.u, i);
    }

    tag = message_read_tag(message);
    EXPECT_EQ(tag.type, mpack_type_map);
    EXPECT_EQ(tag.v.n, 3);

    {
        tag = message_read_tag(message);
        EXPECT_EQ(tag.type, mpack_type_str);
        data = message_read_bytes(message, tag.v.l);
        str = std::string{data, tag.v.l};
        EXPECT_STREQ(str.c_str(), "one");

        tag = message_read_tag(message);
        EXPECT_EQ(tag.type, mpack_type_uint);
        EXPECT_EQ(tag.v.u, 1);

        tag = message_read_tag(message);
        EXPECT_EQ(tag.type, mpack_type_str);
        data = message_read_bytes(message, tag.v.l);
        str = std::string{data, tag.v.l};
        EXPECT_STREQ(str.c_str(), "two");

        tag = message_read_tag(message);
        EXPECT_EQ(tag.type, mpack_type_uint);
        EXPECT_EQ(tag.v.u, 2);

        tag = message_read_tag(message);
        EXPECT_EQ(tag.type, mpack_type_str);
        data = message_read_bytes(message, tag.v.l);
        str = std::string{data, tag.v.l};
        EXPECT_STREQ(str.c_str(), "three");

        tag = message_read_tag(message);
        EXPECT_EQ(tag.type, mpack_type_uint);
        EXPECT_EQ(tag.v.u, 3);
    }

    message_destroy(message);

    EXPECT_EQ(arena->header->read_offset, 0);
    EXPECT_EQ(arena->header->write_offset, 0);

    transport_shm_destroy(transport);
}
