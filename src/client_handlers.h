#ifndef CLIENT_HANDLERS_H
#define CLIENT_HANDLERS_H

#include <netinet/in.h>
#include <tlo/darray.h>

#define NUM_CLIENT_HANDLER_PTHREADS 4
#define NUM_CLIENT_CLEANER_PTHREADS 1

typedef struct ClientHandler {
  // private
  TloDArray clientPtrs;
  int numUnhandledClients;
  int numClosedClients;
  pthread_mutex_t clientsMutex;
  pthread_cond_t clientsUnhandled;
  pthread_cond_t clientsClosed;
  pthread_t clientHandlers[NUM_CLIENT_HANDLER_PTHREADS];
  pthread_t clientCleaner;
  bool continueHandling;
} ClientHandler;

#define CLIENT_HANDLERS_ERROR -1
#define CLIENT_HANDLERS_SUCCESS 0

int clientHandlersInit(ClientHandler *handler);
int clientHandlersAddClient(ClientHandler *handler, int fd,
                            const char *addressString, in_port_t port);
void clientHandlersCleanup(ClientHandler *handler);

#endif  // CLIENT_HANDLERS_H
