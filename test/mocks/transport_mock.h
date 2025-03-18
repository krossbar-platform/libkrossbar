#pragma once

#include <array>
#include <deque>

extern "C"
{
    #include <message_writer.h>
    #include <message.h>
    #include <transport.h>
}

static constexpr size_t MESSAGE_SIZE = 128;

class TransportMock;

class MessageWriterMock: public kb_message_writer_t
{
public:
    MessageWriterMock(TransportMock *transport, log4c_category_t *logger);

    static int send_impl(struct kb_message_writer_s *writer);

private:
    TransportMock *m_transport;
    std::array<uint8_t, MESSAGE_SIZE> m_data = {};
};

class MessageMock: public kb_message_t
{
public:
    MessageMock(std::array<uint8_t, MESSAGE_SIZE> &&data);

    static int destroy_impl(struct kb_message_s *message);

private:
    std::array<uint8_t, MESSAGE_SIZE> m_data;
};

class TransportMock: public kb_transport_s
{
public:
    TransportMock(log4c_category_t *logger, const char *name);

    static kb_message_writer_t *message_init_impl(struct kb_transport_s *transport);
    static kb_message_t *message_receive_impl(struct kb_transport_s *transport);

    void add_message(std::array<uint8_t, MESSAGE_SIZE> &&message);

private:
    std::deque<std::array<uint8_t, MESSAGE_SIZE>> m_messages = {};
};