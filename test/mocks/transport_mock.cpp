#include "transport_mock.h"

#include <mpack.h>

MessageWriterMock::MessageWriterMock(TransportMock *transport, log4c_category_t *logger) : m_transport(transport)
{
    m_data.fill(0);
    message_writer_init(this, m_data.data(), m_data.size(), logger);
    send = send_impl;
}

int MessageWriterMock::send_impl(struct kb_message_writer_s *writer)
{
    auto self = reinterpret_cast<MessageWriterMock *>(writer);

    self->m_transport->add_message(std::move(self->m_data));
    delete self;
    return 0;
}

MessageMock::MessageMock(std::array<uint8_t, MESSAGE_SIZE> &&data): m_data(std::move(data))
{
    message_init(this, m_data.data(), m_data.size());
    destroy = destroy_impl;
}

int MessageMock::destroy_impl(struct kb_message_s *message)
{
    delete (MessageMock *)message;

    return 0;
}

TransportMock::TransportMock(log4c_category_t *logger,
                const char *name)
{
    logger = logger;
    name = name;
    message_init = message_init_impl;
    message_receive = message_receive_impl;
}

kb_message_writer_t *TransportMock::TransportMock::message_init_impl(struct kb_transport_s *transport)
{
    auto self = (TransportMock *)transport;

    return new MessageWriterMock(self, self->logger);
}

kb_message_t *TransportMock::message_receive_impl(struct kb_transport_s *transport)
{
    auto self = (TransportMock *)transport;

    if (self->m_messages.empty())
    {
        return nullptr;
    }

    MessageMock *message = new MessageMock(std::move(self->m_messages.front()));
    self->m_messages.pop_front();
    return message;
}

void TransportMock::add_message(std::array<uint8_t, MESSAGE_SIZE> &&message)
{
    m_messages.push_back(message);
}
