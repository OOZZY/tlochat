#include "client_handlers.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <tlo/thread.h>
#include <tlo/util.h>
#include <unistd.h>

typedef enum ClientState {
  CLIENT_UNHANDLED,
  CLIENT_BEING_HANDLED,
  CLIENT_CLOSED
} ClientState;

typedef struct Client {
  int fd;
  char addressString[INET6_ADDRSTRLEN];
  in_port_t port;
  ClientState state;
} Client;

#define RECEIVE_BUFFER_SIZE 1024
#define SEND_BUFFER_SIZE 1024

static void sendToAllClients(ClientHandler *handler, const char *message,
                             int messageLen) {
  pthread_mutex_lock(&handler->clientsMutex);
  for (size_t i = 0; i < tloDArrayGetSize(&handler->clientPtrs); ++i) {
    Client **clientPtrPtr = tloDArrayGetMutableElement(&handler->clientPtrs, i);
    if ((*clientPtrPtr)->state != CLIENT_CLOSED) {
      Client *clientPtr = *clientPtrPtr;
      int numBytesSent = send(clientPtr->fd, message, messageLen, MSG_NOSIGNAL);
      if (numBytesSent == -1) {
        perror("tlochat client handlers: send");
        fprintf(stderr,
                "tlochat client handlers: failed to send message to %s|%u\n",
                clientPtr->addressString, clientPtr->port);
      }
    }
  }
  pthread_mutex_unlock(&handler->clientsMutex);
}

static void *handleClients(void *data) {
  ClientHandler *handler = data;

  while (handler->continueHandling) {
    Client *clientPtr = NULL;

    pthread_mutex_lock(&handler->clientsMutex);
    while (handler->continueHandling && handler->numUnhandledClients == 0) {
      errno =
          pthread_cond_wait(&handler->clientsUnhandled, &handler->clientsMutex);
      assert(!errno);
    }

    for (size_t i = 0; i < tloDArrayGetSize(&handler->clientPtrs); ++i) {
      Client **clientPtrPtr =
          tloDArrayGetMutableElement(&handler->clientPtrs, i);
      if ((*clientPtrPtr)->state == CLIENT_UNHANDLED) {
        (*clientPtrPtr)->state = CLIENT_BEING_HANDLED;
        clientPtr = *clientPtrPtr;
        handler->numUnhandledClients--;
        break;
      }
    }
    pthread_mutex_unlock(&handler->clientsMutex);

    if (!clientPtr) {
      continue;
    }

    char receiveBuffer[RECEIVE_BUFFER_SIZE];
    bool clientClosed = false;

    while (handler->continueHandling) {
      // printf("tlochat client handlers: receiving from %s|%u\n",
      // clientPtr->addressString, clientPtr->port);
      ssize_t numBytesReceived =
          recv(clientPtr->fd, receiveBuffer, RECEIVE_BUFFER_SIZE - 1, 0);
      if (numBytesReceived == 0) {
        printf("tlochat client handlers: %s|%u closed connection\n",
               clientPtr->addressString, clientPtr->port);
        clientClosed = true;
        break;
      }
      if (numBytesReceived < 0) {
        // printf("tlochat client handlers: %s|%u timed out\n",
        // clientPtr->addressString, clientPtr->port);
        break;
      }
      receiveBuffer[numBytesReceived] = '\0';

      printf("%s|%u: %s", clientPtr->addressString, clientPtr->port,
             receiveBuffer);

      char sendBuffer[SEND_BUFFER_SIZE];
      snprintf(sendBuffer, SEND_BUFFER_SIZE, "%s|%u: %s",
               clientPtr->addressString, clientPtr->port, receiveBuffer);
      sendToAllClients(handler, sendBuffer, strlen(sendBuffer) + 1);
    }

    if (clientClosed) {
      close(clientPtr->fd);
      printf("tlochat client handlers: closed connection from %s|%u\n",
             clientPtr->addressString, clientPtr->port);
      clientPtr->state = CLIENT_CLOSED;

      pthread_mutex_lock(&handler->clientsMutex);
      handler->numClosedClients++;
      errno = pthread_cond_signal(&handler->clientsClosed);
      assert(!errno);
      pthread_mutex_unlock(&handler->clientsMutex);
    } else {
      // printf("tlochat client handlers: defer connection from %s|%u\n",
      // clientPtr->addressString, clientPtr->port);
      clientPtr->state = CLIENT_UNHANDLED;

      pthread_mutex_lock(&handler->clientsMutex);
      handler->numUnhandledClients++;
      errno = pthread_cond_signal(&handler->clientsUnhandled);
      assert(!errno);
      pthread_mutex_unlock(&handler->clientsMutex);
    }
  }

  pthread_exit(NULL);
}

#define IGNORE_OUT_ARG NULL

static void *cleanClients(void *data) {
  ClientHandler *handler = data;

  while (handler->continueHandling) {
    pthread_mutex_lock(&handler->clientsMutex);
    while (handler->continueHandling && handler->numClosedClients == 0) {
      errno =
          pthread_cond_wait(&handler->clientsClosed, &handler->clientsMutex);
      assert(!errno);
    }

    printf("tlochat client handlers: %zu clients\n",
           tloDArrayGetSize(&handler->clientPtrs));
    printf("tlochat client handlers: cleaning up clients\n");
    for (size_t i = 0; i < tloDArrayGetSize(&handler->clientPtrs); ++i) {
      Client **clientPtrPtr =
          tloDArrayGetMutableElement(&handler->clientPtrs, i);
      if ((*clientPtrPtr)->state == CLIENT_CLOSED) {
        tloDArrayUnorderedRemove(&handler->clientPtrs, i);
        i--;
        handler->numClosedClients--;
      }
    }
    printf("tlochat client handlers: %zu clients\n",
           tloDArrayGetSize(&handler->clientPtrs));
    pthread_mutex_unlock(&handler->clientsMutex);
  }

  pthread_exit(NULL);
}

#define DEFAULT_ATTRIBUTES NULL
#define NO_ARGS NULL

int clientHandlersInit(ClientHandler *handler) {
  assert(handler);

  handler->numUnhandledClients = 0;
  handler->numClosedClients = 0;
  handler->clientsMutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  handler->clientsUnhandled = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  handler->clientsClosed = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
  handler->continueHandling = true;

  printf("tlochat client handlers: initializing client handlers\n");
  TloError tloError =
      tloDArrayConstruct(&handler->clientPtrs, &tloPtr, NULL, 0);
  if (tloError) {
    fprintf(stderr, "tlochat client handlers: tloDArrayConstruct failed\n");
    return CLIENT_HANDLERS_ERROR;
  }

  int error =
      tloCreateThreads(handler->clientHandlers, NUM_CLIENT_HANDLER_PTHREADS,
                       handleClients, handler);
  if (error) {
    tloDArrayDestruct(&handler->clientPtrs);
    perror("tlochat client handlers: tloCreateThreads");
    return CLIENT_HANDLERS_ERROR;
  }

  error = tloCreateThreads(&handler->clientCleaner, NUM_CLIENT_CLEANER_PTHREADS,
                           cleanClients, handler);
  if (error) {
    tloCancelThreads(handler->clientHandlers, NUM_CLIENT_HANDLER_PTHREADS);
    tloDArrayDestruct(&handler->clientPtrs);
    perror("tlochat client handlers: tloCreateThreads");
    return CLIENT_HANDLERS_ERROR;
  }

  return CLIENT_HANDLERS_SUCCESS;
}

int clientHandlersAddClient(ClientHandler *handler, int fd,
                            const char *addressString, in_port_t port) {
  assert(handler);

  Client *client = malloc(sizeof(Client));
  if (!client) {
    perror("tlochat client handlers: malloc");
    return CLIENT_HANDLERS_ERROR;
  }

  client->fd = fd;
  strcpy(client->addressString, addressString);
  client->port = port;
  client->state = CLIENT_UNHANDLED;

  pthread_mutex_lock(&handler->clientsMutex);
  TloError tloError = tloDArrayMoveBack(&handler->clientPtrs, &client);
  if (!tloError) {
    handler->numUnhandledClients++;
    errno = pthread_cond_signal(&handler->clientsUnhandled);
    assert(!errno);
  }
  pthread_mutex_unlock(&handler->clientsMutex);

  if (tloError) {
    free(client);
    fprintf(stderr, "tlochat client handlers: tloDArrayMoveBack failed\n");
    return CLIENT_HANDLERS_ERROR;
  }

  return CLIENT_HANDLERS_SUCCESS;
}

void clientHandlersCleanup(ClientHandler *handler) {
  assert(handler);

  printf("tlochat client handlers: cleaning up client handlers\n");
  handler->continueHandling = false;

  // stop all threads from waiting
  errno = pthread_cond_broadcast(&handler->clientsUnhandled);
  assert(!errno);
  errno = pthread_cond_broadcast(&handler->clientsClosed);
  assert(!errno);

  tloJoinThreads(&handler->clientCleaner, NUM_CLIENT_CLEANER_PTHREADS);
  tloJoinThreads(handler->clientHandlers, NUM_CLIENT_HANDLER_PTHREADS);
  tloDArrayDestruct(&handler->clientPtrs);

  errno = pthread_mutex_destroy(&handler->clientsMutex);
  assert(!errno);
  errno = pthread_cond_destroy(&handler->clientsUnhandled);
  assert(!errno);
  errno = pthread_cond_destroy(&handler->clientsClosed);
  assert(!errno);
}
