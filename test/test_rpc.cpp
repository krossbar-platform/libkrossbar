#include <deque>

#include <gtest/gtest.h>
#include <mpack.h>

#include "mocks/transport_mock.h"

extern "C"
{
#include <rpc.h>
}

TEST(Rpc, TestRpcCall)
{
    auto logger = log4c_category_get("libkrossbar.test");

    TransportMock transport(logger, "test");
    auto rpc_writer = rpc_init(&transport, logger);
    auto rpc_reader = rpc_init(&transport, logger);

    auto outgoing_message = rpc_message(rpc_writer);
    ASSERT_EQ(message_write_u16(outgoing_message, 42), KB_MESSAGE_OK);
    ASSERT_EQ(message_send(outgoing_message), 0);

    auto incoming_message = transport_message_receive(&transport);
    ASSERT_NE(incoming_message, nullptr);

    auto rpc_message = rpc_handle_incoming_message(rpc_reader, incoming_message);
    ASSERT_NE(rpc_message, nullptr);

    ASSERT_EQ(rpc_message->type, KB_MESSAGE_TYPE_MESSAGE);
    ASSERT_NE(rpc_message->message, nullptr);

    auto tag = message_read_tag(rpc_message->message);
    ASSERT_EQ(tag.type, mpack_type_uint);
    ASSERT_EQ(tag.v.u, 42);

    rpc_message_release(rpc_message);
    rpc_destroy(rpc_writer);
    rpc_destroy(rpc_reader);
}