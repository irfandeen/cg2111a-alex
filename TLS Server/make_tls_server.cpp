#include "make_tls_server.h"
#include <signal.h>

// Exit flag.  We quit the listener loop when this is 1
static volatile int _exitFlag = 0;

// CTRL-C handler
void breakHandler(int dummy) {
    printf("\n\nEXITING. WHY DID YOU PRESS CTRL-C??\n\n");
    _exitFlag = 1;
}

// SIGTERM handler
void termHandler(int dummy) {
    printf("\n\nEXITING. WHY DID YOU TERMINATE ME??\n\n");
    _exitFlag = 1;
}

// SIGKILL handler
void killHandler(int dummy) {
    printf("\n\nEXITING. YOU KILLER!\n\n");
    _exitFlag = 1;
}
// Maximum length of a filename, including
// path.

#define MAX_FILENAME_LEN 128
#define MAX_DOMAIN_NAME_LEN 128

// Thread structure for the listener thread
static pthread_t _listener;

// This variable points to the worker thread, and _tls_listener
// uses it to spawn the new worker thread

static void *(*_worker_thread)(void *);

// Our port number
static int _port_num;

// Our private key and certificate filenames
static char _key_filename[MAX_FILENAME_LEN];
static char _cert_filename[MAX_FILENAME_LEN];

// Set whether or not to do verification
static int _verify_peer = 0;

// Our CA's cert name.
static char _ca_cert_filename[MAX_FILENAME_LEN];

// Client's  name, as entered into the FQDN field
// of their certificate
static char _client_name[MAX_DOMAIN_NAME_LEN];

// This function is the internal server listener loop. It
// creates a new socket, then listens to it, then spawns
// a worker thread if there is a new connection.

static void *_tls_listener(void *dummy) {

    // Declare two integer variables that will
    // point to sockets.
    int listenfd, connfd;

    // serv_addr will be used to configure the port number
    // of our server.
    struct sockaddr_in serv_addr;

    // Set every element in serv_addr to 0.
    memset(&serv_addr, 0, sizeof(serv_addr));
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    // We use perror to print out error messages
    if (listenfd < 0) {
        perror("Unable to create socket: ");
        exit(-1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(_port_num);

    int one = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    // Now actually bind our socket to port 5001
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to bind: ");
        exit(-1);
    }

    printf("Now listening..\n");
    if (listen(listenfd, 10) < 0) {
        perror("Unable to listen to port: ");
        exit(-1);
    }

    int c = sizeof(struct sockaddr_in);

    // NEW: Initialize the SSL library
    init_openssl();

    // NEW: Create SSL Context.
    SSL_CTX *ctx = create_context(_ca_cert_filename, _verify_peer, 1);

    // NEW: Configure context to load certificate and private keys
    configure_context(ctx, _cert_filename, _key_filename);

    // NEW: Configure multithreading in OpenSSL
    CRYPTO_thread_setup();

    // This exit variable is declared globally and is set
    // by the CTRL-C handler.
    while (!_exitFlag) {
        // Accept a new connection from the queue in listen. We will
        // build an echo server
        struct sockaddr_in client;
        connfd = accept(listenfd, (struct sockaddr *)&client, (socklen_t *)&c);

        char clientAddress[32];

        // Use inet_ntop to extract client's IP address.
        inet_ntop(AF_INET, &client.sin_addr, clientAddress, 32);

        printf("Received connection from %s\n", clientAddress);

        SSL *ssl;

        if (_verify_peer) {
            ssl = connectSSL(ctx, connfd, _client_name);
        } else {
            ssl = connectSSL(ctx, connfd, NULL);
        }

        if (ssl != NULL) {
            int spawn = 1;

            if (_verify_peer) {
                printCertificate(ssl);

                if (!verifyCertificate(ssl)) {
                    printf("Certificate error for %s\n", clientAddress);
                    spawn = 0;
                } else
                    printf("SSL CLIENT CERTIFICATE IS VALID.\n");
            }

            if (spawn) {
                pthread_t worker;

                // NEW: We pass in ssl instead of connfd
                pthread_create(&worker, NULL, _worker_thread, (void *)ssl);
                pthread_detach(worker);
            }
        }
    }

    // We reach here if our program exits
    close(listenfd);
    SSL_CTX_free(ctx);
    thread_cleanup();
    cleanup_openssl();

    return NULL;
}

void createServer(const char *keyFilename, const char *certFilename, int portNum, void *(*workerThread)(void *), const char *caCertFilename, const char *clientName, int verifyPeer) {


    _worker_thread = workerThread;
    _port_num = portNum;

    strncpy(_key_filename, keyFilename, MAX_FILENAME_LEN);
    strncpy(_cert_filename, certFilename, MAX_FILENAME_LEN);

    _verify_peer = verifyPeer;
    if (verifyPeer) {
        strncpy(_ca_cert_filename, caCertFilename, MAX_FILENAME_LEN);
        strncpy(_client_name, clientName, MAX_DOMAIN_NAME_LEN);
    }

    // Set the handlers
    signal(SIGINT, breakHandler);
    signal(SIGTERM, termHandler);
    signal(SIGKILL, killHandler);

    // Now spawn the listener thread

    printf("\n** Spawning TLS Server **\n\n");
    pthread_create(&_listener, NULL, _tls_listener, NULL);
    pthread_detach(_listener);
}

// Returns TRUE if the TLS listener loop is still running.
int server_is_running() {
    return !_exitFlag;
}
