#ifndef __MAKE_TLS_CLIENT__
#define __MAKE_TLS_CLIENT__

#include "tls_client_lib.h"
#include "tls_common_lib.h"
#include "tls_pthread.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void createClient(const char *serverName, const int serverPort, int verifyServer, const char *caCertFname, const char *serverNameOnCert, int sendCert, const char *clientCertFname, const char *clientPrivateKey,
                  void *(*readerThread)(void *), void *(*writerThread)(void *));
// Call this function to check if the client loop is still running
int client_is_running();

// Call this function to kill the client loop
void stopClient();
#endif
