#include "constants.h"
#include "make_tls_server.h"
#include "netconstants.h"
#include "packet.h"
#include "serial.h"
#include "serialize.h"
#include "tls_common_lib.h"

// Ports for Arduino
#define PORT_NAME "/dev/ttyACM0"
#define PORT_NAME2 "/dev/ttyACM1"

// Baud Rate for Arduino
#define BAUD_RATE B9600

// TLS Port Number
#define SERVER_PORT 5001

// Certificate/Keys Names
#define KEY_FNAME "alex.key"
#define CERT_FNAME "alex.crt"
#define CA_CERT_FNAME "signing.pem"
#define CLIENT_NAME "yours.com"

#define BUF_LEN 129
static volatile int networkActive;
static void *tls_conn = NULL;

/*

    Alex Serial Routines to the Arduino

    */

// Prototype for sendNetworkData
void sendNetworkData(const char *, int);

void handleErrorResponse(TPacket *packet) {
    printf("UART ERROR: %d\n", packet->command);
    char buffer[2];
    buffer[0] = NET_ERROR_PACKET;
    buffer[1] = packet->command;
    sendNetworkData(buffer, sizeof(buffer));
}

void handleMessage(TPacket *packet) {
    char data[33];
    printf("UART MESSAGE PACKET: %s\n", packet->data);
    data[0] = NET_MESSAGE_PACKET;
    memcpy(&data[1], packet->data, sizeof(packet->data));
    sendNetworkData(data, sizeof(data));
}

void handleStatus(TPacket *packet) {
    char data[65];
    printf("UART STATUS PACKET\n");
    data[0] = NET_STATUS_PACKET;
    memcpy(&data[1], packet->params, sizeof(packet->params));
    sendNetworkData(data, sizeof(data));
}

void handleResponse(TPacket *packet) {
    // The response code is stored in command
    switch (packet->command) {
    case RESP_OK:
        char resp[2];
        printf("Command OK\n");
        resp[0] = NET_ERROR_PACKET;
        resp[1] = RESP_OK;
        sendNetworkData(resp, sizeof(resp));
        break;

    case RESP_STATUS:
        handleStatus(packet);
        break;

    default:
        printf("Boo\n");
    }
}

void handleUARTPacket(TPacket *packet) {
    switch (packet->packetType) {
    case PACKET_TYPE_COMMAND:
        // Only we send command packets, so ignore
        break;

    case PACKET_TYPE_RESPONSE:
        handleResponse(packet);
        break;

    case PACKET_TYPE_ERROR:
        handleErrorResponse(packet);
        break;

    case PACKET_TYPE_MESSAGE:
        handleMessage(packet);
        break;
    }
}

void uartSendPacket(TPacket *packet) {
    char buffer[PACKET_SIZE];
    int len = serialize(buffer, packet, sizeof(TPacket));

    serialWrite(buffer, len);
}

void handleError(TResult error) {
    switch (error) {
    case PACKET_BAD:
        printf("ERROR: Bad Magic Number\n");
        break;

    case PACKET_CHECKSUM_BAD:
        printf("ERROR: Bad checksum\n");
        break;

    default:
        printf("ERROR: UNKNOWN ERROR\n");
    }
}

void *uartReceiveThread(void *p) {
    char buffer[PACKET_SIZE];
    int len;
    TPacket packet;
    TResult result;
    int counter = 0;

    while (1) {
        len = serialRead(buffer);
        counter += len;
        if (len > 0) {
            result = deserialize(buffer, len, &packet);

            if (result == PACKET_OK) {
                counter = 0;
                handleUARTPacket(&packet);
            } else if (result != PACKET_INCOMPLETE) {
                printf("PACKET ERROR\n");
                handleError(result);
            }
        }
    }
}

void sendNetworkData(const char *data, int len) {
    if (networkActive) {
        int c;

        printf("WRITING TO CLIENT\n");

        if (tls_conn != NULL) {
            c = sslWrite(tls_conn, data, len);
            if (c < 0) perror("Error writing to network (Alex Server)\n");
            networkActive = (c > 0);
        }
    }
}

void handleCommand(void *conn, const char *buffer) {
    // The first byte contains the command
    char cmd = buffer[1];
    uint32_t cmdParam[2];

    // Copy over the parameters.
    memcpy(cmdParam, &buffer[2], sizeof(cmdParam));

    TPacket commandPacket;

    commandPacket.packetType = PACKET_TYPE_COMMAND;
    commandPacket.params[0] = cmdParam[0];
    commandPacket.params[1] = cmdParam[1];

    printf("COMMAND RECEIVED: %c %d %d\n", cmd, cmdParam[0], cmdParam[1]);

    switch (cmd) {
    case 'w':
    case 'W':
        commandPacket.command = COMMAND_FORWARD;
        uartSendPacket(&commandPacket);
        break;

    case 's':
    case 'S':
        commandPacket.command = COMMAND_REVERSE;
        uartSendPacket(&commandPacket);
        break;

    case 'a':
    case 'A':
        commandPacket.command = COMMAND_TURN_LEFT;
        uartSendPacket(&commandPacket);
        break;

    case 'd':
    case 'D':
        commandPacket.command = COMMAND_TURN_RIGHT;
        uartSendPacket(&commandPacket);
        break;

    case 'p':
    case 'P':
        commandPacket.command = COMMAND_STOP;
        uartSendPacket(&commandPacket);
        break;

    case 'c':
    case 'C':
        commandPacket.command = COMMAND_CLEAR_STATS;
        commandPacket.params[0] = 0;
        uartSendPacket(&commandPacket);
        break;

    case 'g':
    case 'G':
        commandPacket.command = COMMAND_GET_STATS;
        uartSendPacket(&commandPacket);
        break;

    case 't':
    case 'T':
        commandPacket.command = COMMAND_MAN_NAV;
        uartSendPacket(&commandPacket);
        break;

    default:
        printf("Bad command\n");
    }
}

void handleNetworkData(void *conn, const char *buffer, int len) {
    tls_conn = conn;
    if (buffer[0] == NET_COMMAND_PACKET)
        handleCommand(conn, buffer);
}

void *worker(void *conn) {
    int len;
    char buffer[BUF_LEN];
    while (networkActive) {
        len = sslRead(conn, buffer, sizeof(buffer));
        networkActive = (len > 0);

        if (len > 0)
            handleNetworkData(conn, buffer, len);
        else if (len < 0)
            perror("ERROR READING NETWORK: ");
    }

    tls_conn = NULL;
    EXIT_THREAD(conn);
}

void sendHello() {
    TPacket helloPacket;
    helloPacket.packetType = PACKET_TYPE_HELLO;
    uartSendPacket(&helloPacket);
}

int main() {
    pthread_t serThread;
    printf("\nALEX REMOTE SUBSYSTEM\n\n");

    printf("Opening Serial Port\n");
    // Open the serial port
    bool status = startSerial(PORT_NAME, BAUD_RATE, 8, 'N', 1, 5);
    if (!status) startSerial(PORT_NAME2, BAUD_RATE, 8, 'N', 1, 5);
    printf("Done. Waiting 3 seconds for Arduino to reboot\n");
    sleep(3);

    printf("DONE. Starting Serial Listener\n");
    pthread_create(&serThread, NULL, uartReceiveThread, NULL);

    printf("Starting Alex Server\n");

    networkActive = 1;
    createServer(KEY_FNAME, CERT_FNAME, SERVER_PORT, &worker, CA_CERT_FNAME, CLIENT_NAME, 1);

    printf("DONE. Sending HELLO to Arduino\n");
    sendHello();
    printf("DONE.\n");

    while (server_is_running());
}
