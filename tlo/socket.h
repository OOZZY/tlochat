#ifndef TLO_SOCKET_H
#define TLO_SOCKET_H

#include <netdb.h>
#include <sys/socket.h>

/*
 * - returns NULL on failure
 */
struct addrinfo *tloGetLocalAddressInfo(const char *portOrService);

#define TLO_SOCKET_ERROR -1

/*
 * - returns TLO_SOCKET_ERROR on failure
 */
int tloGetSocketThenBind(struct addrinfo *infos);

void *tloGetAddress(struct sockaddr *socket);
in_port_t tloGetPort(struct sockaddr *socket);

#endif  // TLO_SOCKET_H
