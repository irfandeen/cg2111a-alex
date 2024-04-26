// Main Arduio Code
// Last Edited 12 April 2024

#include <math.h>
#include <stdarg.h>

#include "constants.h"
#include "data.h"
#include "packet.h"
#include "serialize.h"

#define ALEX_LENGTH 25.5
#define ALEX_BREADTH 15

#define TIMEOUT 1400
#define MIN_DIST 20.0

#define NUDGE_DIST 5.0
#define NUDGE_SPEED 50.0
#define TURN_ANG 25.0
#define TURN_SPEED 100.0

float alexDiagonal = 0.0;
float alexCirc = 0.0;

// if MAN_NAV is false, then control with WASD, if true, then f 100 100, etc.
volatile bool MAN_NAV = true;

#define COUNTS_PER_REV 4
#define WHEEL_CIRC 6.5 * PI

volatile TDirection dir;

// Store the ticks from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicks;
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks;
volatile unsigned long rightReverseTicks;

// Left and right encoder ticks for turning
volatile unsigned long leftForwardTicksTurns;
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns;
volatile unsigned long rightReverseTicksTurns;

// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRevs;
volatile unsigned long rightRevs;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;

// Variables to keep track of whether we've moved a commanded distance
unsigned long deltaDist;
unsigned long newDist;

// Variables to keep track of our turning angle
unsigned long deltaTicks;
unsigned long targetTicks;

unsigned long computeDeltaTicks(float ang) {
    unsigned long ticks = (unsigned long)((ang * alexCirc * COUNTS_PER_REV) / (360.0 * WHEEL_CIRC));
    return ticks;
}

void left(float ang, float speed) {
    if (ang == 0)
        deltaTicks = 9999999;
    else
        deltaTicks = computeDeltaTicks(ang);
    targetTicks = leftReverseTicksTurns + deltaTicks;
    ccw(ang, speed);
}

void right(float ang, float speed) {
    if (ang == 0)
        deltaTicks = 9999999;
    else
        deltaTicks = computeDeltaTicks(ang);
    targetTicks = rightReverseTicksTurns + deltaTicks;
    cw(ang, speed);
}

/*
 *
 * Alex Communication Routines.
 *
 */

TResult readPacket(TPacket *packet) {
    char buffer[PACKET_SIZE];
    int len;
    len = readSerial(buffer);
    if (len == 0)
        return PACKET_INCOMPLETE;
    else
        return deserialize(buffer, len, packet);
}

void sendStatus() {
    TPacket statuspacket;
    statuspacket.packetType = PACKET_TYPE_RESPONSE;
    statuspacket.command = RESP_STATUS;
    float front_ultra = getUltra(FRONT);
    float rear_ultra = getUltra(REAR);

    // Sensor Data & Manual/Auto Mode Data
    statuspacket.params[0] = getColour();
    statuspacket.params[1] = MAN_NAV; // tempRed;
    statuspacket.params[2] = front_ultra;
    statuspacket.params[3] = rear_ultra;

    // Odometry Data
    statuspacket.params[4] = leftForwardTicks;
    statuspacket.params[5] = rightForwardTicks;
    statuspacket.params[6] = leftReverseTicks;
    statuspacket.params[7] = rightReverseTicks;
    statuspacket.params[8] = leftForwardTicksTurns;
    statuspacket.params[9] = rightForwardTicksTurns;
    statuspacket.params[10] = leftReverseTicksTurns;
    statuspacket.params[11] = rightReverseTicksTurns;
    statuspacket.params[12] = forwardDist;
    statuspacket.params[13] = reverseDist;

    sendResponse(&statuspacket);
}

void sendMessage(const char *message) {
    TPacket messagePacket;
    messagePacket.packetType = PACKET_TYPE_MESSAGE;
    strncpy(messagePacket.data, message, MAX_STR_LEN);
    sendResponse(&messagePacket);
}

void dbprintf(char *format, ...) {
    va_list args;
    char buffer[128];

    va_start(args, format);
    vsprintf(buffer, format, args);
    sendMessage(buffer);
}

void sendBadPacket() {
    // Tell the Pi that it sent us a packet with a bad
    // magic number.

    TPacket badPacket;
    badPacket.packetType = PACKET_TYPE_ERROR;
    badPacket.command = RESP_BAD_PACKET;
    sendResponse(&badPacket);
}

void sendBadChecksum() {
    TPacket badChecksum;
    badChecksum.packetType = PACKET_TYPE_ERROR;
    badChecksum.command = RESP_BAD_CHECKSUM;
    sendResponse(&badChecksum);
}

void sendBadCommand() {
    TPacket badCommand;
    badCommand.packetType = PACKET_TYPE_ERROR;
    badCommand.command = RESP_BAD_COMMAND;
    sendResponse(&badCommand);
}

void sendBadResponse() {
    TPacket badResponse;
    badResponse.packetType = PACKET_TYPE_ERROR;
    badResponse.command = RESP_BAD_RESPONSE;
    sendResponse(&badResponse);
}

void sendOK() {
    TPacket okPacket;
    okPacket.packetType = PACKET_TYPE_RESPONSE;
    okPacket.command = RESP_OK;
    sendResponse(&okPacket);
}

void sendResponse(TPacket *packet) {
    char buffer[PACKET_SIZE];
    int len;
    len = serialize(buffer, packet, sizeof(TPacket));
    writeSerial(buffer, len);
}

void enablePullups() {
    DDRD &= 0b11110011;
    PORTD |= 0b00001100;
}

// Functions to be called by INT2 and INT3 ISRs.
void leftISR() {
    if (dir == FORWARD) leftForwardTicks++;
    if (dir == BACKWARD) leftReverseTicks++;
    if (dir == LEFT) leftReverseTicksTurns++;
    if (dir == RIGHT) leftForwardTicksTurns++;
    if (dir == FORWARD) forwardDist = (unsigned long)((float)leftForwardTicks / COUNTS_PER_REV * WHEEL_CIRC);
    if (dir == BACKWARD) reverseDist = (unsigned long)((float)leftReverseTicks / COUNTS_PER_REV * WHEEL_CIRC);
}

void rightISR() {
    if (dir == FORWARD) rightForwardTicks++;
    if (dir == BACKWARD) rightReverseTicks++;
    if (dir == RIGHT) rightReverseTicksTurns++;
    if (dir == LEFT) rightForwardTicksTurns++;
}

void setupEINT() {
    EIMSK |= 0b00001100;
    EICRA |= 0b10100000;
}

ISR(INT2_vect) {
    rightISR();
}

ISR(INT3_vect) {
    leftISR();
}

void setupSerial() {
    Serial.begin(9600);
}

int readSerial(char *buffer) {
    int count = 0;
    while (Serial.available())
        buffer[count++] = Serial.read();
    return count;
}
void writeSerial(const char *buffer, int len) {
    Serial.write(buffer, len);
}

// Clears all our counters
void clearCounters() {
    leftForwardTicks = 0;
    rightForwardTicks = 0;
    leftReverseTicks = 0;
    rightReverseTicks = 0;
    leftForwardTicksTurns = 0;
    rightForwardTicksTurns = 0;
    leftReverseTicksTurns = 0;
    rightReverseTicksTurns = 0;
    leftRevs = 0;
    rightRevs = 0;
    forwardDist = 0;
    reverseDist = 0;
}

// Clears one particular counter
void clearOneCounter(int which) {
    clearCounters();
}
// Intialize Alex's internal states

void initializeState() {
    clearCounters();
}

void handleCommand(TPacket *command) {
    switch (command->command) {
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_FORWARD:
        sendOK();
        if (MAN_NAV) forward((double)command->params[0], (float)command->params[1]);
        else         forward(NUDGE_DIST, NUDGE_SPEED);
        break;

    case COMMAND_REVERSE:
        sendOK();
        if (MAN_NAV) backward((double)command->params[0], (float)command->params[1]);
        else         backward(NUDGE_DIST, NUDGE_SPEED);
        break;

    case COMMAND_TURN_LEFT:
        sendOK();
        if (MAN_NAV) left((double)command->params[0], (float)command->params[1]);
        else         left(TURN_ANG, TURN_SPEED);
        break;

    case COMMAND_TURN_RIGHT:
        sendOK();
        if (MAN_NAV) right((double)command->params[0], (float)command->params[1]);
        else         right(TURN_ANG, TURN_SPEED);
        break;

    case COMMAND_STOP:
        sendOK();
        stop();
        break;

    case COMMAND_GET_STATS:
        sendStatus();
        break;

    case COMMAND_CLEAR_STATS:
        sendOK();
        clearOneCounter(command->params[0]);
        break;

    case COMMAND_MAN_NAV:
        MAN_NAV = !MAN_NAV;
        break;

    default:
        sendBadCommand();
    }
}

void waitForHello() {
    int exit = 0;
    while (!exit) {
        TPacket hello;
        TResult result;
        do {
            result = readPacket(&hello);
        } while (result == PACKET_INCOMPLETE);

        if (result == PACKET_OK) {
            if (hello.packetType == PACKET_TYPE_HELLO) {
                sendOK();
                exit = 1;
            } else
                sendBadResponse();
        } else if (result == PACKET_BAD) sendBadPacket();
        else if (result == PACKET_CHECKSUM_BAD)
            sendBadChecksum();
    } // !exit
}

void setup() {
    alexDiagonal = sqrt((ALEX_LENGTH * ALEX_LENGTH) + (ALEX_BREADTH * ALEX_BREADTH));
    alexCirc = PI * alexDiagonal;

    cli();
    setupEINT();
    setupSerial();
    enablePullups();
    initializeState();
    sei();
    colourSetup();
}

void handlePacket(TPacket *packet) {
    switch (packet->packetType) {
    case PACKET_TYPE_COMMAND:
        handleCommand(packet);
        break;

    case PACKET_TYPE_RESPONSE:
        break;

    case PACKET_TYPE_ERROR:
        break;

    case PACKET_TYPE_MESSAGE:
        break;

    case PACKET_TYPE_HELLO:
        break;
    }
}

void loop() {
    TPacket recvPacket;

    TResult result = readPacket(&recvPacket);
    if (result == PACKET_OK) {
        handlePacket(&recvPacket);
        char tmp[] = "THE PACKET IS OKAY!";
        dbprintf(tmp);
    } else if (result == PACKET_BAD) {
        char tmp[] = "THE PACKET IS BAD!!";
        dbprintf(tmp);
        sendBadPacket();
    } else if (result == PACKET_CHECKSUM_BAD) {
        char tmp[] = "THE CHECKSUM IS BAD!!";
        dbprintf(tmp);
        sendBadChecksum();
    }
    if (deltaDist > 0) {
        if (dir == FORWARD) {
            if (forwardDist > newDist) {
                deltaDist = 0;
                newDist = 0;
                stop();
            }
        } else if (dir == BACKWARD) {
            if (reverseDist > newDist) {
                deltaDist = 0;
                newDist = 0;
                stop();
            }
        } else if (dir == (TDirection)STOP) {
            deltaDist = 0;
            newDist = 0;
            stop();
        }
    }
    if (deltaTicks > 0) {
        if (dir == LEFT) {
            if (leftReverseTicksTurns >= targetTicks) {
                deltaTicks = 0;
                stop();
            }
        } else if (dir == RIGHT) {
            if (rightReverseTicksTurns >= targetTicks) {
                deltaTicks = 0;
                targetTicks = 0;
                stop();
            }
        } else if (dir == (TDirection)STOP) {
            deltaTicks = 0;
            targetTicks = 0;
            stop();
        }
    }
}
/*}
}
}*/
