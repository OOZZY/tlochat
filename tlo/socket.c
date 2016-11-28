#include "tlo/socket.h"
#include <assert.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct addrinfo *tloGetLocalAddressInfo(const char *portOrService) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *localAddressInfo;

  int error = getaddrinfo(NULL, portOrService, &hints, &localAddressInfo);
  if (error) {
    fprintf(stderr, "tlochat socket: getaddrinfo: %s\n", gai_strerror(error));
    return NULL;
  }

  return localAddressInfo;
}

int tloGetSocketThenBind(struct addrinfo *infos) {
  int socketfd;
  bool succeeded = false;

  for (struct addrinfo *info = infos; info != NULL; info = info->ai_next) {
    socketfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (socketfd == -1) {
      perror("tlochat socket: socket");
      continue;
    }

    int value = 1;
    int error =
        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
    if (error) {
      close(socketfd);
      perror("tlochat socket: setsockopt");
      continue;
    }

    error = bind(socketfd, info->ai_addr, info->ai_addrlen);
    if (error) {
      close(socketfd);
      perror("tlochat socket: bind");
      continue;
    }

    succeeded = true;
    break;
  }

  if (succeeded) {
    return socketfd;
  } else {
    return TLO_SOCKET_ERROR;
  }
}

void *tloGetAddress(struct sockaddr *socket) {
  if (socket->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)socket)->sin_addr);
  }

  if (socket->sa_family == AF_INET6) {
    return &(((struct sockaddr_in6 *)socket)->sin6_addr);
  }

  assert(false);
}

in_port_t tloGetPort(struct sockaddr *socket) {
  if (socket->sa_family == AF_INET) {
    return ((struct sockaddr_in *)socket)->sin_port;
  }

  if (socket->sa_family == AF_INET6) {
    return ((struct sockaddr_in6 *)socket)->sin6_port;
  }

  assert(false);
}
