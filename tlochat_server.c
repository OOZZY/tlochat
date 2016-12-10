#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <tlo/socket.h>
#include <unistd.h>
#include "client_handlers.h"

#define PORT "12345"
#define MAX_NUM_PENDING_CONNECTIONS 10
//#define RECEIVE_BUFFER_SIZE 1024

static bool continueListening = true;

static void sigintHandler(int signal) {
  (void)signal;
  continueListening = false;
}

#define NUM_SECONDS_RECEIVE_TIMEOUT 10
#define NUM_USECONDS_RECEIVE_TIMEOUT 0

int main(void) {
  struct sigaction sa;
  sa.sa_handler = sigintHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  int error = sigaction(SIGINT, &sa, NULL);
  if (error) {
    perror("tlochat server: sigaction");
    exit(EXIT_FAILURE);
  }

  struct addrinfo *localAddressInfo = tloGetBindableWildcardAddress(PORT);
  if (!localAddressInfo) {
    exit(EXIT_FAILURE);
  }

  printf("tlochat server: binding to one of the following socket addresses:\n");
  tloPrintAddressInformation(localAddressInfo);

  int serverfd = tloGetSocketBoundToReusableAddress(localAddressInfo);
  freeaddrinfo(localAddressInfo);
  if (serverfd == TLO_SOCKET_ERROR) {
    exit(EXIT_FAILURE);
  }

  error = listen(serverfd, MAX_NUM_PENDING_CONNECTIONS);
  if (error) {
    close(serverfd);
    perror("tlochat server: listen");
    exit(EXIT_FAILURE);
  }

  ClientHandler handler;
  error = clientHandlersInit(&handler);
  if (error) {
    close(serverfd);
    fprintf(stderr, "tlochat server: clientHandlersInit failed\n");
    exit(EXIT_FAILURE);
  }

  while (continueListening) {
    printf("tlochat server: waiting for connections\n");
    struct sockaddr_storage clientSocket;
    socklen_t clientSocketLen = sizeof(clientSocket);
    int clientfd =
        accept(serverfd, (struct sockaddr *)&clientSocket, &clientSocketLen);
    if (clientfd == -1) {
      perror("tlochat server: accept");
      continue;
    }

    struct timeval tv;
    tv.tv_sec = NUM_SECONDS_RECEIVE_TIMEOUT;    // Secs Timeout
    tv.tv_usec = NUM_USECONDS_RECEIVE_TIMEOUT;  // Not init'ing this can cause
                                                // strange errors
    error = setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                       sizeof(struct timeval));
    if (error) {
      close(clientfd);
      perror("tlochat server: setsockopt");
      continue;
    }

    char clientAddressString[INET6_ADDRSTRLEN];
    inet_ntop(clientSocket.ss_family,
              tloGetAddress((struct sockaddr *)&clientSocket),
              clientAddressString, INET6_ADDRSTRLEN);

    in_port_t clientPort = tloGetPort((struct sockaddr *)&clientSocket);

    printf("tlochat server: got connection from %s|%u\n", clientAddressString,
           clientPort);

    // char receiveBuffer[RECEIVE_BUFFER_SIZE];

    // while (continueListening) {
    //  ssize_t numBytesReceived =
    //      recv(clientfd, receiveBuffer, RECEIVE_BUFFER_SIZE - 1, 0);
    //  if (numBytesReceived == 0) {
    //    break;
    //  }
    //  receiveBuffer[numBytesReceived] = '\0';

    //  printf("%s|%u: %s", clientAddressString, clientPort, receiveBuffer);
    //}

    // close(clientfd);
    // printf("tlochat server: closed connection from %s|%u\n",
    // clientAddressString, clientPort);

    error = clientHandlersAddClient(&handler, clientfd, clientAddressString,
                                    clientPort);
    if (error) {
      fprintf(stderr, "tlochat server: clientHandlersAddClient failed\n");
    }
  }

  printf("tlochat server: received sigint\n");
  clientHandlersCleanup(&handler);
  close(serverfd);

  printf("tlochat server: exiting successfully\n");
  exit(EXIT_SUCCESS);
}
