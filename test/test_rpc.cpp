#include <deque>

#include <gtest/gtest.h>
#include <mpack.h>

extern "C" {
    #include <uds/transport_uds.h>
    #include <uds/message_uds.h>
    #include <uds/message_writer_uds.h>
    #include <rpc.h>
}

static constexpr size_t MESSAGE_SIZE = 128;

class MessageWriterMock: public kb_message_writer_t
{
public:
    MessageWriterMock(TransportMock *transport, log4c_category_t *logger) : m_transport(transport)
    {
        message_writer_init(this, m_data.data(), m_data.size(), logger);
        send = send_impl;
    }

    ~MessageWriterMock()
    {
        mpack_writer_destroy(data_writer);
        free(data_writer);
    }

    static int send_impl(struct kb_message_writer_s *writer)
    {
        auto self = reinterpret_cast<MessageWriterMock *>(writer);

        self->m_transport->add_message(std::move(self->m_data));
        delete self;
        return 0;
    }

private:
    TransportMock *m_transport;
    std::array<uint8_t, MESSAGE_SIZE> m_data = {};
};

class MessageMock: public kb_message_t
{
public:
    MessageMock(std::array<uint8_t, MESSAGE_SIZE> &&data): m_data(std::move(data))
    {
        message_init(this, m_data.data(), m_data.size());
        destroy = destroy_impl;
    }

    static int destroy_impl(struct kb_message_s *message)
    {
        mpack_reader_destroy(message->data_reader);
        free(message->data_reader);
    }

private:
    std::array<uint8_t, MESSAGE_SIZE> m_data;
};

class TransportMock: public kb_transport_s
{
public:
    TransportMock(log4c_category_t *logger,
                   const char *name)
    {
        logger = logger;
        name = name;
        message_init = message_init_impl;
        message_receive = message_receive_impl;
    }

    static kb_message_writer_t *TransportMock::message_init_impl(struct kb_transport_s *transport)
    {
        auto self = reinterpret_cast<TransportMock *>(transport);

        return new MessageWriterMock(self, self->logger);
    }

    static kb_message_t *message_receive_impl(struct kb_transport_s *transport)
    {
        auto self = reinterpret_cast<TransportMock *>(transport);

        if (self->m_messages.empty())
        {
            return nullptr;
        }

        MessageMock *message = new MessageMock(std::move(self->m_messages.front()));
        self->m_messages.pop_front();
        return message;
    }

    void add_message(std::array<uint8_t, MESSAGE_SIZE> &&message)
    {
        m_messages.push_back(message);
    }

private:
    std::deque<std::array<uint8_t, MESSAGE_SIZE>> m_messages = {};
};

TEST(Rpc, TestRpcCall)
{
    auto logger = log4c_category_get("libkrossbar.test");

    TransportMock transport(logger, "test");
    auto rpc_writer = rpc_init(&transport, logger);
    auto rpc_reader = rpc_init(&transport, logger);

    auto outgoing_message = rpc_message(rpc_writer);
    message_write_u16(outgoing_message, 42);
    message_send(outgoing_message);
}