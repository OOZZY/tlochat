#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "client_handlers.h"
#include "tlo/socket.h"

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

  struct addrinfo *localAddressInfo = tloGetLocalAddressInfo(PORT);
  if (!localAddressInfo) {
    exit(EXIT_FAILURE);
  }

  int socketfd = tloGetSocketThenBind(localAddressInfo);
  freeaddrinfo(localAddressInfo);
  if (socketfd == TLO_SOCKET_ERROR) {
    exit(EXIT_FAILURE);
  }

  struct timeval tv;
  tv.tv_sec = NUM_SECONDS_RECEIVE_TIMEOUT;    // Secs Timeout
  tv.tv_usec = NUM_USECONDS_RECEIVE_TIMEOUT;  // Not init'ing this can cause
                                              // strange errors
  error = setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                     sizeof(struct timeval));
  if (error) {
    close(socketfd);
    perror("tlochat server: setsockopt");
    exit(EXIT_FAILURE);
  }

  error = listen(socketfd, MAX_NUM_PENDING_CONNECTIONS);
  if (error) {
    close(socketfd);
    perror("tlochat server: listen");
    exit(EXIT_FAILURE);
  }

  error = clientHandlersInit();
  if (error) {
    close(socketfd);
    fprintf(stderr, "tlochat server: clientHandlersAddClient failed\n");
    exit(EXIT_FAILURE);
  }

  printf("tlochat server: waiting for connections\n");

  while (continueListening) {
    struct sockaddr_storage clientSocket;
    socklen_t clientSocketLen = sizeof(clientSocket);
    int clientfd =
        accept(socketfd, (struct sockaddr *)&clientSocket, &clientSocketLen);
    if (clientfd == -1) {
      perror("tlochat server: accept");
      continue;
    }

    char clientAddressString[INET6_ADDRSTRLEN];
    inet_ntop(clientSocket.ss_family,
              tloGetAddress((struct sockaddr *)&clientSocket),
              clientAddressString, INET6_ADDRSTRLEN);

    in_port_t clientPort = tloGetPort((struct sockaddr *)&clientSocket);

    printf("tlochat server: got connection from %s:%u\n", clientAddressString,
           clientPort);

    // char receiveBuffer[RECEIVE_BUFFER_SIZE];

    // while (continueListening) {
    //  ssize_t numBytesReceived =
    //      recv(clientfd, receiveBuffer, RECEIVE_BUFFER_SIZE - 1, 0);
    //  if (numBytesReceived == 0) {
    //    break;
    //  }
    //  receiveBuffer[numBytesReceived] = '\0';

    //  printf("%s:%u: %s", clientAddressString, clientPort, receiveBuffer);
    //}

    // close(clientfd);
    // printf("tlochat server: closed connection from %s:%u\n",
    // clientAddressString, clientPort);

    error = clientHandlersAddClient(clientfd, clientAddressString, clientPort);
    if (error) {
      fprintf(stderr, "tlochat server: clientHandlersAddClient failed\n");
    }
  }

  printf("tlochat server: received sigint\n");
  clientHandlersCleanup();
  close(socketfd);

  printf("tlochat server: exiting successfully\n");
  exit(EXIT_SUCCESS);
}
