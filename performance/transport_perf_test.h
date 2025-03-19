#pragma once

#include <cstddef>
#include <future>
#include <chrono>

#include <liburing.h>
#include <log4c.h>

extern "C"
{
#include "transport.h"
}

class TransportPerfTestRunner
{
public:
    enum class TransportType
    {
        UDS,
        SHMEM
    };

    TransportPerfTestRunner(size_t message_size, size_t message_count, TransportType transport_type, log4c_category_t *logger);
    ~TransportPerfTestRunner();

    std::chrono::microseconds run();

private:
    std::future<void> start_sending();
    std::future<void> start_receiving();

    kb_transport_t *m_sender_transport;
    kb_transport_t *m_receiver_transport;
    struct io_uring m_sender_ring;
    struct io_uring m_receiver_ring;

    size_t m_message_size;
    size_t m_message_count;
};