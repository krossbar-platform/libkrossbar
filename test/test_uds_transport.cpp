#include <string>

#include <gtest/gtest.h>
#include <liburing.h>

extern "C" {
#include <uds/transport_uds.h>
#include <uds/message_uds.h>
#include <uds/message_writer_uds.h>
}

static constexpr size_t MESSAGE_SIZE = 128;
static constexpr size_t MAX_MESSAGE_NUM = 10;
static constexpr size_t RING_QUEUE_DEPTH = 32;

static constexpr size_t BUFFER_SIZE = MESSAGE_SIZE - 2;
static char RANDOM_BUFFER[BUFFER_SIZE] = {};

// Function to set socket to non-blocking mode
static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    ASSERT_NE(flags, -1);
    ASSERT_NE(fcntl(fd, F_SETFL, flags | O_NONBLOCK), -1);
}


static void wait_readable(struct io_uring *ring, int fd)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    io_uring_prep_recv(sqe, fd, NULL, 0, 0);

    int ret = io_uring_submit(ring);
    if (ret < 0)
    {
        fprintf(stderr, "io_uring futex wait submit error: %s\n", strerror(-ret));
    }

    struct io_uring_cqe *cqe;
    __kernel_timespec timeout = {0, 20000000};

    ASSERT_EQ(io_uring_wait_cqe_timeout(ring, &cqe, &timeout), 0);
    io_uring_cqe_seen(ring, cqe);
}

TEST(Transport, TestUDSTransport) {
    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    int sockets[2];
    // Create socket pair
    ASSERT_NE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), -1);
    set_nonblocking(sockets[0]);
    set_nonblocking(sockets[1]);

    auto transport_writer = transport_uds_init("test", sockets[0], MESSAGE_SIZE, MAX_MESSAGE_NUM, &ring);

    auto message_writer = transport_message_init(transport_writer);

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
    ASSERT_EQ(transport_uds_write_messages(transport_writer), 0);

    auto transport_reader = transport_uds_init("test_reader", sockets[1], MESSAGE_SIZE, MAX_MESSAGE_NUM, &ring);

    wait_readable(&ring, sockets[1]);
    auto message = transport_message_receive(transport_reader);

    ASSERT_NE(message, nullptr);

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

    ASSERT_EQ(transport_message_receive(transport_writer), nullptr);

    transport_destroy(transport_writer);
    transport_destroy(transport_reader);
}

TEST(Transport, TestUDSTransportMultisend)
{
    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    int sockets[2];
    // Create socket pair
    ASSERT_NE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), -1);
    set_nonblocking(sockets[0]);
    set_nonblocking(sockets[1]);

    auto transport_writer = transport_uds_init("test", sockets[0], MESSAGE_SIZE, MAX_MESSAGE_NUM, &ring);

    auto message_writer = transport_message_init(transport_writer);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(transport_writer);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    message_writer = transport_message_init(transport_writer);
    message_write_bin(message_writer, RANDOM_BUFFER, BUFFER_SIZE);
    ASSERT_EQ(message_send(message_writer), 0);

    ASSERT_EQ(transport_uds_write_messages(transport_writer), 0);

    auto transport_reader = transport_uds_init("test", sockets[1], MESSAGE_SIZE, MAX_MESSAGE_NUM, &ring);

    wait_readable(&ring, sockets[1]);

    auto message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    auto tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_bin);
    ASSERT_EQ(tag.v.l, BUFFER_SIZE);
    message_destroy(message);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_bin);
    ASSERT_EQ(tag.v.l, BUFFER_SIZE);
    message_destroy(message);

    message = transport_message_receive(transport_reader);
    ASSERT_NE(message, nullptr);
    tag = message_read_tag(message);
    ASSERT_EQ(tag.type, mpack_type_bin);
    ASSERT_EQ(tag.v.l, BUFFER_SIZE);
    message_destroy(message);

    message = transport_message_receive(transport_reader);
    ASSERT_EQ(message, nullptr);

    transport_destroy(transport_writer);
    transport_destroy(transport_reader);
}
