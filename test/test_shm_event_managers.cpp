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
}

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

    struct io_uring ring;
    ASSERT_EQ(io_uring_queue_init(RING_QUEUE_DEPTH, &ring, 0), 0);

    struct io_uring_probe *probe;

    // Allocate and initialize probe
    probe = io_uring_get_probe_ring(&ring);
    if (!probe) {
        fprintf(stderr, "Failed to get probe: %s\n", strerror(errno));
        return;
    }

    ASSERT_TRUE(io_uring_opcode_supported(probe, IORING_OP_FUTEX_WAIT));
    ASSERT_TRUE(io_uring_opcode_supported(probe, IORING_OP_FUTEX_WAKE));
    free(probe);

    auto transport_writer = transport_shm_init("test", ARENA_SIZE, MESSAGE_SIZE, &ring);
    auto transport_reader = transport_shm_connect("test_reader", transport_shm_get_fd(transport_writer), &ring);
    auto reader_event_manager = &((kb_transport_shm_t *)transport_reader)->event_manager;

    auto message = transport_message_receive(transport_reader);

    ASSERT_EQ(message, nullptr);
    event_manager_shm_wait_messages(reader_event_manager);

    auto future = std::async(std::launch::async, send_message, transport_writer);

    struct io_uring_cqe *cqe;
    __kernel_timespec timeout = {0, 40000000};
    ASSERT_EQ(io_uring_wait_cqe_timeout(&ring, &cqe, &timeout), 0);

    if (cqe->res < 0)
    {
        fprintf(stderr, "Futex wait error: %d, %s\n", -cqe->res, strerror(-cqe->res));
    }
    ASSERT_EQ(cqe->res, 0);

    auto event_manager = (kb_event_manager_t*)io_uring_cqe_get_data(cqe);
    auto received_message = event_manager_shm_handle_event(cqe);

    ASSERT_NE(received_message, nullptr);
    message_destroy(received_message);

    io_uring_cqe_seen(&ring, cqe);

    transport_destroy(transport_writer);
    transport_destroy(transport_reader);
    future.wait();
}
