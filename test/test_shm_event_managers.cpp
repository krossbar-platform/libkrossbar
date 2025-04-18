#if defined(IO_URING_FUTEXES)

#include <thread>
#include <future>

#include <gtest/gtest.h>
#include <liburing.h>
#include <log4c.h>

#include <unistd.h>
#include <fcntl.h>

#include <shmem/transport_shm.h>
#include <shmem/message_shm.h>
#include <shmem/message_writer_shm.h>

static constexpr size_t ARENA_SIZE = 512;
static constexpr size_t MESSAGE_SIZE = 128;
static constexpr size_t RING_QUEUE_DEPTH = 32;


TEST(EventManagers, TestShmemEventManager)
{
    auto send_message = [](kb_transport_t *transport)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto message_writer = transport_message_init(transport);
        message_write_bool(message_writer, true);
        ASSERT_EQ(message_send(message_writer), 0);
    };

    auto logger = log4c_category_get("libkrossbar.test");
    auto map_fd_0 = transport_shm_create_mapping("map0", ARENA_SIZE, logger);
    auto map_fd_1 = transport_shm_create_mapping("map1", ARENA_SIZE, logger);

    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    auto transport_writer = transport_shm_init("test_writer", map_fd_0, map_fd_1, MESSAGE_SIZE, &ring, logger);
    auto transport_reader = transport_shm_init("test_reader", map_fd_1, map_fd_0, MESSAGE_SIZE, &ring, logger);
    auto reader_event_manager = (kb_event_manager_shm_t *)transport_reader->event_manager;

    auto message = transport_message_receive(transport_reader);

    ASSERT_EQ(message, nullptr);
    event_manager_shm_wait_messages(reader_event_manager);

    auto future = std::async(std::launch::async, send_message, transport_writer);

    struct io_uring_cqe *cqe;
    __kernel_timespec timeout = {0, 40000000};
    ASSERT_EQ(io_uring_wait_cqe_timeout(&ring, &cqe, &timeout), 0);

    if (cqe->res < 0)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "Futex cqe error: %d, %s\n", -cqe->res, strerror(-cqe->res));
    }
    ASSERT_EQ(cqe->res, 0);

    auto received_message = event_manager_shm_handle_event(cqe);

    ASSERT_NE(received_message, nullptr);
    message_destroy(received_message);

    io_uring_cqe_seen(&ring, cqe);

    transport_destroy(transport_writer);
    transport_destroy(transport_reader);
    future.wait();
}

#endif
