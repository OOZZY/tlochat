#ifndef CLIENT_HANDLERS_H
#define CLIENT_HANDLERS_H

#include <netinet/in.h>

#define CLIENT_HANDLERS_ERROR -1
#define CLIENT_HANDLERS_SUCCESS 0

int clientHandlersInit();
int clientHandlersAddClient(int fd, const char *addressString, in_port_t port);
void clientHandlersCleanup();

#endif  // CLIENT_HANDLERS_H
