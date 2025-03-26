#pragma once

#include <cstddef>
#include <future>
#include <chrono>

#include <liburing.h>
#include <log4c.h>

extern "C"
{
#include "transport.h"
#include "event_manager.h"
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

    void start_event_loop(struct io_uring *ring, kb_event_manager_t *event_manager);
    std::chrono::microseconds run();

private:
    std::future<void> start_sending();
    std::future<void> start_receiving();

    kb_transport_t *m_sender_transport;
    kb_transport_t *m_receiver_transport;
    struct io_uring m_sender_ring;
    struct io_uring m_receiver_ring;
    log4c_category_t *m_logger;

    size_t m_message_size;
    size_t m_message_count;
};