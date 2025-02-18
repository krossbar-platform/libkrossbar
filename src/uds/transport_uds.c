#include "transport_uds.h"

#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#include <mpack-writer.h>
#include <utlist.h>

#include "message_writer_uds.h"
#include "message_uds.h"

kb_transport_t *transport_uds_init(const char *name, int fd, size_t max_message_size, size_t max_buffered_messages, struct io_uring *ring)
{
    kb_transport_uds_t *transport = calloc(1, sizeof(kb_transport_uds_t));
    if (transport == NULL)
    {
        perror("calloc failed");
        return NULL;
    }

    transport->name = strdup(name);
    transport->max_message_size = max_message_size;
    transport->max_buffered_messages = max_buffered_messages;
    transport->sock_fd = fd;

    event_manager_uds_init(&transport->event_manager, transport, ring);

    transport->base.message_init = transport_uds_message_init;
    transport->base.message_receive = transport_uds_message_receive;
    transport->base.get_fd = transport_uds_get_fd;
    transport->base.destroy = transport_uds_destroy;

    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    return (kb_transport_t *)transport;
}

kb_message_writer_t *transport_uds_message_init(kb_transport_t *transport)
{
    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    if (self->out_message_count >= self->max_buffered_messages)
    {
        return NULL;
    }

    char *buffer = malloc(self->max_message_size);
    if (!buffer)
    {
        perror("malloc failed");
        return NULL;
    }

    kb_message_writer_uds_t *writer = message_writer_uds_init(self, buffer, self->max_message_size);

    return &writer->base;
}

int transport_uds_message_send(kb_transport_t *transport, kb_message_writer_t *writer)
{
    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;
    kb_message_writer_uds_t *writer_uds = (kb_message_writer_uds_t *)writer;

    size_t message_size = writer_message_size(writer);
    char *data = writer->data_writer->buffer;

    // Shrink message to fre some extra memory
    data = realloc(data, message_size);
    out_messages_t *out_message = calloc(1, sizeof(out_messages_t));
    out_message->message.data = data;
    out_message->message.data_size = message_size;

    DL_APPEND(self->out_messages, out_message);

    self->out_message_count++;
}

kb_message_t *transport_uds_message_receive(kb_transport_t *transport)
{

    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    // Receiving a new message
    if (self->in_message.data == NULL)
    {
        uint64_t read_size = 0;

        // Check if we can read message size
        if (recv(self->sock_fd, &read_size, sizeof(read_size), MSG_PEEK) != sizeof(uint64_t))
        {
            return NULL;
        }

        recv(self->sock_fd, &read_size, sizeof(read_size), 0);
        self->in_message.data_size = read_size;
        self->in_message.current_offset = 0;
        self->in_message.data = malloc(read_size);
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
            kb_message_uds_t *message = message_uds_init(self, self->in_message.data, self->in_message.data_size);
            return &message->base;
        }
    }

    return NULL;
}

int transport_uds_message_release(kb_transport_t *transport, kb_message_t *message)
{
    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    free(self->in_message.data);
    self->in_message.data = NULL;
    self->in_message.data_size = 0;

    return 0;
}

int transport_uds_get_fd(kb_transport_t *transport)
{
    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    return self->sock_fd;
}

void transport_uds_destroy(kb_transport_t *transport)
{
    kb_transport_uds_t *self = (kb_transport_uds_t *)transport;

    if (self->sock_fd != 0)
    {
        close(self->sock_fd);
    }

    if (self->name != NULL)
    {
        free((char *)self->name);
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

    free(self);
}