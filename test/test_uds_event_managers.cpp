#include <thread>
#include <future>

#include <gtest/gtest.h>
#include <liburing.h>

#include <unistd.h>
#include <fcntl.h>

extern "C" {
#include <shmem/transport_shm.h>
#include <shmem/message_shm.h>
#include <shmem/message_writer_shm.h>
#include <uds/transport_uds.h>
#include <uds/message_uds.h>
#include <uds/message_writer_uds.h>
}

static constexpr size_t ARENA_SIZE = 512;
static constexpr size_t MESSAGE_SIZE = 128;
static constexpr size_t RING_QUEUE_DEPTH = 32;

// Function to set socket to non-blocking mode
int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
        return -1;
    }
    return 0;
}

TEST(EventManagers, TestUDSEventManager)
{
    auto send_message = [](kb_transport_t *transport)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto message_writer = transport_message_init(transport);
        message_write_bool(message_writer, true);
        ASSERT_EQ(message_send(message_writer), 0);
    };

    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    int sockets[2];
    // Create socket pair
    ASSERT_NE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), -1);
    ASSERT_NE(set_nonblocking(sockets[0]), -1);
    ASSERT_NE(set_nonblocking(sockets[1]), -1);

    auto transport_writer = transport_uds_init("test_writer", sockets[0], MESSAGE_SIZE, 10, &ring);
    auto transport_reader = transport_uds_init("test_reader", sockets[1], MESSAGE_SIZE, 10, &ring);
    auto reader_event_manager = &((kb_transport_uds_t *)transport_reader)->event_manager;

    auto message = transport_message_receive(transport_reader);

    ASSERT_EQ(message, nullptr);
    event_manager_uds_wait_readable(reader_event_manager);

    auto future = std::async(std::launch::async, send_message, transport_writer);

    struct io_uring_cqe *cqe;
    __kernel_timespec timeout = {0, 20000000};
    ASSERT_EQ(io_uring_wait_cqe_timeout(&ring, &cqe, &timeout), 0);

    if (cqe->res < 0)
    {
        fprintf(stderr, "Futex wait error: %d, %s\n", -cqe->res, strerror(-cqe->res));
    }
    ASSERT_EQ(cqe->res, 0);

    auto event_manager = (kb_event_manager_t *)io_uring_cqe_get_data(cqe);
    auto received_message = event_manager_uds_handle_event(event_manager, cqe);

    ASSERT_NE(received_message, nullptr);

    io_uring_cqe_seen(&ring, cqe);
}
