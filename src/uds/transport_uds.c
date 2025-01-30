#include "transport_uds.h"

#include <sys/socket.h>
#include <fcntl.h>

#include "message_writer_uds.h"

kb_transport_t *transport_uds_init(const char *name, int fd, size_t max_message_size, struct io_uring *ring)
{
    kb_transport_uds_t *transport = calloc(1, sizeof(kb_transport_uds_t));
    if (transport == NULL)
    {
        perror("calloc failed");
        return NULL;
    }

    transport->name = strdup(name);
    transport->max_message_size = max_message_size;

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
}

kb_message_t *transport_uds_message_receive(kb_transport_t *transport);
int transport_uds_message_release(kb_transport_t *transport, kb_message_t *message);

int transport_uds_get_fd(kb_transport_t *transport);
void transport_uds_destroy(kb_transport_t *transport);