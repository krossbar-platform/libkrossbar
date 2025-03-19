#include "transport_perf_test.h"

#include <iostream>

#include <liburing.h>
#include <log4c.h>

extern "C"
{
#include "uds/transport_uds.h"
#include "uds/event_manager_uds.h"
#include "shmem/transport_shm.h"
#include "shmem/event_manager_shm.h"
}

static constexpr size_t ARENA_SIZE = 10000000;
static constexpr size_t RING_QUEUE_DEPTH = 32;

namespace
{
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
}

TransportPerfTestRunner::TransportPerfTestRunner(size_t message_size, size_t message_count, TransportType transport_type, log4c_category_t *logger)
    : m_message_size(message_size), m_message_count(message_count)
{
    io_uring_queue_init(RING_QUEUE_DEPTH, &m_sender_ring, 0);
    io_uring_queue_init(RING_QUEUE_DEPTH, &m_receiver_ring, 0);

    switch (transport_type)
    {
    case TransportType::UDS:
    {
        int sockets[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
        set_nonblocking(sockets[0]);
        set_nonblocking(sockets[1]);

        m_sender_transport = transport_uds_init("test_reader", sockets[0], message_size * 1.2, 100, &m_sender_ring, logger);
        m_receiver_transport = transport_uds_init("test_reader", sockets[1], message_size * 1.2, 100, &m_receiver_ring, logger);

        break;
    }
    case TransportType::SHMEM:
    {
        auto map_fd_0 = transport_shm_create_mapping("map0", ARENA_SIZE, logger);
        auto map_fd_1 = transport_shm_create_mapping("map1", ARENA_SIZE, logger);

        m_sender_transport = transport_shm_init("sender", map_fd_0, map_fd_1, message_size * 1.2, &m_sender_ring, logger);
        m_receiver_transport = transport_shm_init("receiver", map_fd_1, map_fd_0, message_size * 1.2, &m_receiver_ring, logger);

        break;
    }
    default:
        exit(1);
    }
}

TransportPerfTestRunner::~TransportPerfTestRunner()
{
    transport_destroy(m_sender_transport);
    transport_destroy(m_receiver_transport);
}

std::chrono::microseconds TransportPerfTestRunner::run()
{
    auto start = std::chrono::high_resolution_clock::now();

    auto sending_future = start_sending();
    auto receiving_future = start_receiving();

    sending_future.wait();
    receiving_future.wait();

    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);
}

std::future<void> TransportPerfTestRunner::start_sending()
{
    return std::async(std::launch::async, [this]()
    {
        std::vector<char> random_buffer {};
        random_buffer.resize(m_message_size);

        for (size_t i = 0; i < m_message_count; i++)
        {
            auto message = transport_message_init(m_sender_transport);

            if (message)
            {
                message_write_bin(message, random_buffer.data(), random_buffer.size());
                message_send(message);
            }
            else
            {
                i--;
            }
        }
    });
}

std::future<void> TransportPerfTestRunner::start_receiving()
{
    return std::async(std::launch::async, [this]()
    {
        for (size_t i = 0; i < m_message_count; i++)
        {
            auto message = transport_message_receive(m_receiver_transport);
            if (message)
            {
                message_destroy(message);
            }
            else
            {
                i--;
            }
        }
    });
}