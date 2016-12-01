#include "client_handlers.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <tlo/darray.h>
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

static TloError clientPtrConstructCopy(void *bytes, const void *data) {
  assert(bytes);
  assert(data);

  memcpy(bytes, data, sizeof(Client *));

  return TLO_SUCCESS;
}

void clientPtrDestruct(void *ptr) {
  Client **clientPtrPtr = ptr;

  if (!clientPtrPtr) {
    return;
  }

  if (!*clientPtrPtr) {
    return;
  }

  free(*clientPtrPtr);
  *clientPtrPtr = NULL;
}

static const TloType clientPtrType = {.sizeOf = sizeof(Client *),
                                      .constructCopy = clientPtrConstructCopy,
                                      .destruct = clientPtrDestruct};

static TloDArray clientPtrs;
static int numUnhandledClients = 0;
static int numClosedClients = 0;
static pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clientsUnhandled = PTHREAD_COND_INITIALIZER;
pthread_cond_t clientsClosed = PTHREAD_COND_INITIALIZER;

#define NUM_CLIENT_HANDLER_PTHREADS 4
static pthread_t clientHandlers[NUM_CLIENT_HANDLER_PTHREADS];

#define RECEIVE_BUFFER_SIZE 1024
static bool continueHandling = true;

static void *handleClients(void *data) {
  (void)data;

  while (continueHandling) {
    Client *clientPtr = NULL;

    pthread_mutex_lock(&clientsMutex);
    while (continueHandling && numUnhandledClients == 0) {
      errno = pthread_cond_wait(&clientsUnhandled, &clientsMutex);
      assert(!errno);
    }

    for (size_t i = 0; i < tloDArrayGetSize(&clientPtrs); ++i) {
      Client **clientPtrPtr = tloDArrayGetMutableElement(&clientPtrs, i);
      if ((*clientPtrPtr)->state == CLIENT_UNHANDLED) {
        (*clientPtrPtr)->state = CLIENT_BEING_HANDLED;
        clientPtr = *clientPtrPtr;
        numUnhandledClients--;
        break;
      }
    }
    pthread_mutex_unlock(&clientsMutex);

    if (!clientPtr) {
      continue;
    }

    char receiveBuffer[RECEIVE_BUFFER_SIZE];
    bool clientClosed = false;

    while (continueHandling) {
      // printf("tlochat client handlers: receiving from %s:%u\n",
      // clientPtr->addressString, clientPtr->port);
      ssize_t numBytesReceived =
          recv(clientPtr->fd, receiveBuffer, RECEIVE_BUFFER_SIZE - 1, 0);
      if (numBytesReceived == 0) {
        printf("tlochat client handlers: %s:%u closed connection\n",
               clientPtr->addressString, clientPtr->port);
        clientClosed = true;
        break;
      }
      if (numBytesReceived < 0) {
        // printf("tlochat client handlers: %s:%u timed out\n",
        // clientPtr->addressString, clientPtr->port);
        break;
      }
      receiveBuffer[numBytesReceived] = '\0';

      printf("%s:%u: %s", clientPtr->addressString, clientPtr->port,
             receiveBuffer);
    }

    if (clientClosed) {
      close(clientPtr->fd);
      printf("tlochat client handlers: closed connection from %s:%u\n",
             clientPtr->addressString, clientPtr->port);
      clientPtr->state = CLIENT_CLOSED;

      pthread_mutex_lock(&clientsMutex);
      numClosedClients++;
      errno = pthread_cond_signal(&clientsClosed);
      assert(!errno);
      pthread_mutex_unlock(&clientsMutex);
    } else {
      // printf("tlochat client handlers: defer connection from %s:%u\n",
      // clientPtr->addressString, clientPtr->port);
      clientPtr->state = CLIENT_UNHANDLED;

      pthread_mutex_lock(&clientsMutex);
      numUnhandledClients++;
      errno = pthread_cond_signal(&clientsUnhandled);
      assert(!errno);
      pthread_mutex_unlock(&clientsMutex);
    }
  }

  pthread_exit(NULL);
}

static void cancelPthreads(pthread_t *pthreads, int numPthreads) {
  for (int i = 0; i < numPthreads; ++i) {
    errno = pthread_cancel(pthreads[i]);
    if (errno) {
      perror("tlochat client handlers: pthread_cancel");
    }
    assert(!errno);
  }
}

#define NUM_CLIENT_CLEANER_PTHREADS 1
static pthread_t clientCleaner;

#define NUM_SECONDS_TO_SLEEP 10
#define NUM_NSECONDS_TO_SLEEP 0
#define IGNORE_OUT_ARG NULL

static void *cleanClients(void *data) {
  (void)data;

  while (continueHandling) {
    const struct timespec interval = {NUM_SECONDS_TO_SLEEP,
                                      NUM_NSECONDS_TO_SLEEP};
    nanosleep(&interval, IGNORE_OUT_ARG);

    pthread_mutex_lock(&clientsMutex);
    while (continueHandling && numClosedClients == 0) {
      errno = pthread_cond_wait(&clientsClosed, &clientsMutex);
      assert(!errno);
    }

    printf("tlochat client handlers: %zu clients\n",
           tloDArrayGetSize(&clientPtrs));
    printf("tlochat client handlers: cleaning up clients\n");
    for (size_t i = 0; i < tloDArrayGetSize(&clientPtrs); ++i) {
      Client **clientPtrPtr = tloDArrayGetMutableElement(&clientPtrs, i);
      if ((*clientPtrPtr)->state == CLIENT_CLOSED) {
        tloDArrayUnorderedRemove(&clientPtrs, i);
        i--;
        numClosedClients--;
      }
    }
    printf("tlochat client handlers: %zu clients\n",
           tloDArrayGetSize(&clientPtrs));
    pthread_mutex_unlock(&clientsMutex);
  }

  pthread_exit(NULL);
}

#define DEFAULT_ATTRIBUTES NULL
#define NO_ARGS NULL

int clientHandlersInit() {
  printf("tlochat client handlers: initializing client handlers\n");
  TloError tloError = tloDArrayConstruct(&clientPtrs, &clientPtrType, NULL, 0);
  if (tloError) {
    fprintf(stderr, "tlochat client handlers: tloDArrayConstruct failed\n");
    return CLIENT_HANDLERS_ERROR;
  }

  for (int i = 0; i < NUM_CLIENT_HANDLER_PTHREADS; ++i) {
    errno = pthread_create(&clientHandlers[i], DEFAULT_ATTRIBUTES,
                           handleClients, NO_ARGS);
    if (errno) {
      cancelPthreads(clientHandlers, i);
      tloDArrayDestruct(&clientPtrs);
      perror("tlochat client handlers: pthread_create");
      return CLIENT_HANDLERS_ERROR;
    }
  }

  errno =
      pthread_create(&clientCleaner, DEFAULT_ATTRIBUTES, cleanClients, NO_ARGS);
  if (errno) {
    cancelPthreads(clientHandlers, NUM_CLIENT_HANDLER_PTHREADS);
    tloDArrayDestruct(&clientPtrs);
    perror("tlochat client handlers: pthread_create");
    return CLIENT_HANDLERS_ERROR;
  }

  return CLIENT_HANDLERS_SUCCESS;
}

int clientHandlersAddClient(int fd, const char *addressString, in_port_t port) {
  Client *client = malloc(sizeof(Client));
  if (!client) {
    perror("tlochat client handlers: malloc");
    return CLIENT_HANDLERS_ERROR;
  }

  client->fd = fd;
  strcpy(client->addressString, addressString);
  client->port = port;
  client->state = CLIENT_UNHANDLED;

  pthread_mutex_lock(&clientsMutex);
  TloError tloError = tloDArrayPushBack(&clientPtrs, &client);
  if (!tloError) {
    numUnhandledClients++;
    errno = pthread_cond_signal(&clientsUnhandled);
    assert(!errno);
  }
  pthread_mutex_unlock(&clientsMutex);

  if (tloError) {
    free(client);
    fprintf(stderr, "tlochat client handlers: tloDArrayPushBack failed\n");
    return CLIENT_HANDLERS_ERROR;
  }

  return CLIENT_HANDLERS_SUCCESS;
}

static void joinPthreads(pthread_t *pthreads, int numPthreads) {
  for (int i = 0; i < numPthreads; ++i) {
    errno = pthread_join(pthreads[i], IGNORE_OUT_ARG);
    if (errno) {
      perror("tlochat client handlers: pthread_join");
    }
    assert(!errno);
  }
}

void clientHandlersCleanup() {
  printf("tlochat client handlers: cleaning up client handlers\n");
  continueHandling = false;

  // stop all threads from waiting
  errno = pthread_cond_broadcast(&clientsUnhandled);
  assert(!errno);
  errno = pthread_cond_broadcast(&clientsClosed);
  assert(!errno);

  joinPthreads(&clientCleaner, NUM_CLIENT_CLEANER_PTHREADS);
  joinPthreads(clientHandlers, NUM_CLIENT_HANDLER_PTHREADS);
  tloDArrayDestruct(&clientPtrs);
}
