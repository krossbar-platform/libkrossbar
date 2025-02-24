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
    int sockets[2];
    auto send_message = [&sockets]()
    {
        struct io_uring ring;
        ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);
        auto transport_writer = transport_uds_init("test_writer", sockets[0], MESSAGE_SIZE, 10, &ring);

        auto message_writer = transport_message_init(transport_writer);
        message_write_bool(message_writer, true);
        ASSERT_EQ(message_send(message_writer), 0);

        struct io_uring_cqe *cqe;
        while (true)
        {
            auto res = io_uring_wait_cqe(&ring, &cqe);

            if (res < 0 || cqe->res < 0)
            {
                printf("Error: %d, %s\n", -cqe->res, strerror(-cqe->res));
                break;
            };

            event_manager_uds_handle_event(cqe);
            io_uring_cqe_seen(&ring, cqe);
        }

        transport_destroy(transport_writer);
    };

    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    // Create socket pair
    ASSERT_NE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), -1);
    ASSERT_NE(set_nonblocking(sockets[0]), -1);
    ASSERT_NE(set_nonblocking(sockets[1]), -1);

    auto transport_reader = transport_uds_init("test_reader", sockets[1], MESSAGE_SIZE, 10, &ring);
    auto reader_event_manager = &((kb_transport_uds_t *)transport_reader)->event_manager;

    auto message = transport_message_receive(transport_reader);

    ASSERT_EQ(message, nullptr);

    auto future = std::async(std::launch::async, send_message);

    struct io_uring_cqe *cqe;
    __kernel_timespec timeout = {0, 40000000};
    int wait_res = io_uring_wait_cqe_timeout(&ring, &cqe, &timeout);
    if (wait_res < 0)
    {
        fprintf(stderr, "Futex wait error: %d, %s\n", -wait_res, strerror(-wait_res));
    }
    ASSERT_EQ(wait_res, 0);

    if (cqe->res < 0)
    {
        fprintf(stderr, "Futex cqe error: %d, %s\n", -cqe->res, strerror(-cqe->res));
    }
    ASSERT_EQ(cqe->res, 0);

    auto event_manager = (kb_event_manager_t *)io_uring_cqe_get_data(cqe);
    auto received_message = event_manager_uds_handle_event(cqe);

    ASSERT_NE(received_message, nullptr);
    message_destroy(received_message);

    io_uring_cqe_seen(&ring, cqe);
    close(sockets[0]);
    close(sockets[1]);

    transport_destroy(transport_reader);
    future.wait();
}
