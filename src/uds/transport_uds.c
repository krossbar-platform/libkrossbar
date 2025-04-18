#include "transport_uds.h"

#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#include <utlist.h>

#include "message_writer_uds.h"
#include "message_uds.h"

static uint8_t MAGIC = 0x42;

kb_transport_t *transport_uds_init(const char *name, int fd, size_t max_message_size, size_t max_buffered_messages,
                                   struct io_uring *ring, log4c_category_t *logger)
{
    assert(name != NULL);
    assert(ring != NULL);
    assert(logger != NULL);

    kb_transport_uds_t *transport = malloc(sizeof(kb_transport_uds_t));
    if (transport == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "malloc failed");
        return NULL;
    }

    transport->out_messages = NULL;
    transport->out_message_count = 0;
    transport->max_message_size = max_message_size;
    transport->max_buffered_messages = max_buffered_messages;
    transport->sock_fd = fd;
    transport->in_message.data = NULL;

    transport->base.name = strdup(name);
    transport->base.logger = logger;
    transport->base.message_init = transport_uds_message_init;
    transport->base.message_receive = transport_uds_message_receive;
    transport->base.destroy = transport_uds_destroy;

    kb_event_manager_uds_t *event_manager = event_manager_uds_create(transport, ring, logger);
    if (event_manager == NULL)
    {
        log4c_category_log(logger, LOG4C_PRIORITY_ERROR, "malloc failed");
        transport_uds_destroy((kb_transport_t *)transport);
        return NULL;
    }
    transport->base.event_manager = (kb_event_manager_t *)event_manager;

    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    log4c_category_log(logger, LOG4C_PRIORITY_DEBUG, "UDS transport `%s` initialized", name);

    return (kb_transport_t *)transport;
}

kb_message_writer_t *transport_uds_message_init(kb_transport_t *transport)
{
    assert(transport != NULL);

    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    if (self->out_message_count >= self->max_buffered_messages)
    {
        return NULL;
    }

    char *buffer = malloc(self->max_message_size);
    if (!buffer)
    {
        log4c_category_log(transport->logger, LOG4C_PRIORITY_ERROR, "malloc failed");
        return NULL;
    }

    kb_message_writer_uds_t *writer = message_writer_uds_init(self, buffer, self->max_message_size);

    return &writer->base;
}

int transport_uds_message_send(kb_transport_t *transport, kb_message_writer_t *writer)
{
    assert(transport != NULL);
    assert(writer != NULL);

    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;
    kb_message_writer_uds_t *writer_uds = (kb_message_writer_uds_t *)writer;

    size_t message_size = message_writer_size(writer);
    char *data = writer->buffer;

    // Shrink message to fre some extra memory
    data = realloc(data, message_size);
    out_messages_t *out_message = malloc(sizeof(out_messages_t));
    out_message->prev = NULL;
    out_message->next = NULL;

    out_message->message.data = data;
    out_message->message.data_size = message_size;
    out_message->message.current_offset = 0;
    out_message->message.header_sent = false;

    DL_APPEND(self->out_messages, out_message);

    if (self->out_message_count == 0)
    {
        event_manager_uds_wait_writeable((kb_event_manager_uds_t *)self->base.event_manager);
    }

    self->out_message_count++;
    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "New uds message in `%s`: %zu messages in the buffer", transport->name, self->out_message_count);
}

int transport_uds_write_messages(kb_transport_t *transport)
{
    assert(transport != NULL);

    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    out_messages_t *out_message, *tmp;
    ssize_t bytes_sent = 0;

    DL_FOREACH_SAFE(self->out_messages, out_message, tmp)
    {
        if (!out_message->message.header_sent)
        {
            message_header_t header = {.magic = MAGIC, .data_len = out_message->message.data_size};
            bytes_sent = send(self->sock_fd, &header, sizeof(header), 0);

            if (bytes_sent == -1)
            {
                log4c_category_log(transport->logger, LOG4C_PRIORITY_ERROR, "send failed: %s", strerror(errno));
                return -1;
            }

            out_message->message.header_sent = true;
        }

        bytes_sent = send(self->sock_fd, out_message->message.data, out_message->message.data_size, 0);

        if (bytes_sent == -1)
        {
            log4c_category_log(transport->logger, LOG4C_PRIORITY_ERROR, "send failed: %s", strerror(errno));
            return -1;
        }

        if (bytes_sent < out_message->message.data_size)
        {
            out_message->message.data += bytes_sent;
            out_message->message.data_size -= bytes_sent;
            return self->out_message_count;
        }

        DL_DELETE(self->out_messages, out_message);
        free(out_message->message.data);
        free(out_message);
        self->out_message_count--;

        log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "Message send from `%s`: %zu messages in the buffer", transport->name, self->out_message_count);
    }

    return self->out_message_count;
}

kb_message_t *transport_uds_message_receive(kb_transport_t *transport)
{
    assert(transport != NULL);

    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    // Receiving a new message
    if (self->in_message.data == NULL)
    {
        message_header_t header;

        // Check if we can read message size
        ssize_t bytes_received = recv(self->sock_fd, &header, sizeof(header), MSG_PEEK);
        if (bytes_received != sizeof(header))
        {
            return NULL;
        }

        // Note: read_size contains the size, but we need to pop size from the socket
        recv(self->sock_fd, &header, sizeof(header), 0);

        if (header.magic != MAGIC)
        {
            log4c_category_log(transport->logger, LOG4C_PRIORITY_ERROR, "Invalid magic number: %d", header.magic);
            return NULL;
        }

        self->in_message.data_size = header.data_len;
        self->in_message.current_offset = 0;
        self->in_message.data = malloc(header.data_len);
    }

    ssize_t bytes_received = recv(self->sock_fd,
                                  self->in_message.data + self->in_message.current_offset,
                                  self->in_message.data_size - self->in_message.current_offset,
                                  0);

    if (bytes_received > 0)
    {
        self->in_message.current_offset += bytes_received;

        if (self->in_message.current_offset == self->in_message.data_size)
        {
            log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "Incoming message for `%s`", transport->name);

            kb_message_uds_t *message = message_uds_init(self, self->in_message.data, self->in_message.data_size);
            return &message->base;
        }
    }

    return NULL;
}

int transport_uds_message_release(kb_transport_t *transport, kb_message_t *message)
{
    assert(transport != NULL);
    assert(message != NULL);

    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    free(self->in_message.data);
    self->in_message.data = NULL;
    self->in_message.data_size = 0;

    return 0;
}

void transport_uds_destroy(kb_transport_t *transport)
{
    assert(transport != NULL);

    log4c_category_log(transport->logger, LOG4C_PRIORITY_DEBUG, "UDS transport `%s` destoryed", transport->name);

    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    if (self->sock_fd != 0)
    {
        close(self->sock_fd);
    }

    if (transport->name != NULL)
    {
        free((char *)transport->name);
    }

    if (self->out_messages != NULL)
    {
        out_messages_t *out_message, *tmp;
        DL_FOREACH_SAFE(self->out_messages, out_message, tmp)
        {
            DL_DELETE(self->out_messages, out_message);
            free(out_message->message.data);
            free(out_message);
        }
    }

    if (self->in_message.data != NULL)
    {
        free(self->in_message.data);
    }

    event_manager_uds_destroy((kb_event_manager_uds_t *)transport->event_manager);

    free(self);
}