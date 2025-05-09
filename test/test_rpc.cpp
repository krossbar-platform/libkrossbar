#include <deque>

#include <gtest/gtest.h>

#include "mocks/transport_mock.h"

#include <rpc.h>

TEST(Rpc, TestRpcMessage)
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

TEST(Rpc, TestRpcCall)
{
    int response_counter = 0;

    auto callback = [](kb_message_t *message, void *context)
    {
        auto tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_uint);
        ASSERT_EQ(tag.v.u, 42);

        int *counter = (int *)context;
        (*counter)++;
    };

    auto logger = log4c_category_get("libkrossbar.test");

    TransportMock transport(logger, "test");
    auto rpc_writer = rpc_init(&transport, logger);
    auto rpc_reader = rpc_init(&transport, logger);

    auto outgoing_message = rpc_call(rpc_writer, callback, &response_counter);
    ASSERT_EQ(message_write_u16(outgoing_message, 42), KB_MESSAGE_OK);
    ASSERT_EQ(message_send(outgoing_message), 0);

    auto incoming_message = transport_message_receive(&transport);
    ASSERT_NE(incoming_message, nullptr);

    auto rpc_message = rpc_handle_incoming_message(rpc_reader, incoming_message);
    ASSERT_NE(rpc_message, nullptr);
    ASSERT_EQ(rpc_message->type, KB_MESSAGE_TYPE_CALL);
    ASSERT_NE(rpc_message->message, nullptr);

    auto tag = message_read_tag(rpc_message->message);
    ASSERT_EQ(tag.type, mpack_type_uint);
    ASSERT_EQ(tag.v.u, 42);

    // Sending a response
    auto response = rpc_message_respond(rpc_message);
    ASSERT_EQ(message_write_u16(response, 42), KB_MESSAGE_OK);
    ASSERT_EQ(message_send(response), 0);

    auto incoming_response = transport_message_receive(&transport);
    ASSERT_NE(incoming_response, nullptr);

    // Handling a response
    auto rpc_response_message = rpc_handle_incoming_message(rpc_writer, incoming_response);
    ASSERT_EQ(rpc_response_message, nullptr);
    ASSERT_EQ(response_counter, 1);

    // Sending a response to already responded call
    auto response2 = rpc_message_respond(rpc_message);
    ASSERT_EQ(message_write_u16(response2, 42), KB_MESSAGE_OK);
    ASSERT_EQ(message_send(response2), 0);

    auto incoming_response2 = transport_message_receive(&transport);
    ASSERT_NE(incoming_response2, nullptr);

    // Handling second response
    auto rpc_response_message2 = rpc_handle_incoming_message(rpc_writer, incoming_response2);
    ASSERT_EQ(rpc_response_message2, nullptr);
    ASSERT_EQ(response_counter, 1);

    rpc_message_release(rpc_message);
    rpc_destroy(rpc_writer);
    rpc_destroy(rpc_reader);
}

TEST(Rpc, TestRpcSubscriptions)
{
    int response_counter = 0;

    auto callback = [](kb_message_t *message, void *context)
    {
        auto tag = message_read_tag(message);
        ASSERT_EQ(tag.type, mpack_type_uint);
        ASSERT_EQ(tag.v.u, 42);

        int *counter = (int *)context;
        (*counter)++;
    };

    auto logger = log4c_category_get("libkrossbar.test");

    TransportMock transport(logger, "test");
    auto rpc_writer = rpc_init(&transport, logger);
    auto rpc_reader = rpc_init(&transport, logger);

    auto outgoing_message = rpc_subscribe(rpc_writer, callback, &response_counter);
    ASSERT_EQ(message_write_u16(outgoing_message, 42), KB_MESSAGE_OK);
    ASSERT_EQ(message_send(outgoing_message), 0);

    auto incoming_message = transport_message_receive(&transport);
    ASSERT_NE(incoming_message, nullptr);

    auto rpc_message = rpc_handle_incoming_message(rpc_reader, incoming_message);
    ASSERT_NE(rpc_message, nullptr);
    ASSERT_EQ(rpc_message->type, KB_MESSAGE_TYPE_SUBSCRIPTION);
    ASSERT_NE(rpc_message->message, nullptr);

    auto tag = message_read_tag(rpc_message->message);
    ASSERT_EQ(tag.type, mpack_type_uint);
    ASSERT_EQ(tag.v.u, 42);

    // Sending a response
    auto response = rpc_message_respond(rpc_message);
    ASSERT_EQ(message_write_u16(response, 42), KB_MESSAGE_OK);
    ASSERT_EQ(message_send(response), 0);

    auto incoming_response = transport_message_receive(&transport);
    ASSERT_NE(incoming_response, nullptr);

    // Handling a response
    auto rpc_response_message = rpc_handle_incoming_message(rpc_writer, incoming_response);
    ASSERT_EQ(rpc_response_message, nullptr);
    ASSERT_EQ(response_counter, 1);

    // Sending a response to already responded call
    auto response2 = rpc_message_respond(rpc_message);
    ASSERT_EQ(message_write_u16(response2, 42), KB_MESSAGE_OK);
    ASSERT_EQ(message_send(response2), 0);

    auto incoming_response2 = transport_message_receive(&transport);
    ASSERT_NE(incoming_response2, nullptr);

    // Handling second response
    auto rpc_response_message2 = rpc_handle_incoming_message(rpc_writer, incoming_response2);
    ASSERT_EQ(rpc_response_message2, nullptr);
    ASSERT_EQ(response_counter, 2);

    rpc_message_release(rpc_message);
    rpc_destroy(rpc_writer);
    rpc_destroy(rpc_reader);
}