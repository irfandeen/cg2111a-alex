#include <avr/io.h>

/* USAGE
 *  Ultrasonic Driver Program
 *  - No setup required from main program
 *
 *  for front distance: call getUltra(FRONT) returns double
 *  for rear distance: cal getUltra(REAR) returns distance
 */

// Rear Ultrasonic Pins
#define TRIG_PIN_REAR (1 << 7) // L
#define ECHO_PIN_REAR (1 << 1) // G
#define TRIG_PIN_REAR_DDR DDRL
#define ECHO_PIN_REAR_DDR DDRG
#define TRIG_PIN_REAR_PORT PORTL
#define ECHO_PIN_REAR_PIN PING
#define ECHO_REAR_DIGITAL 40 // for pulseIn

// Front Ultrasonic Pins
#define TRIG_PIN_FRONT (1 << 3) // D25
#define ECHO_PIN_FRONT (1 << 5) // D27
#define TRIG_PIN_FRONT_DDR DDRA
#define ECHO_PIN_FRONT_DDR DDRA
#define TRIG_PIN_FRONT_PORT PORTA
#define ECHO_PIN_FRONT_PIN PINA
#define ECHO_FRONT_DIGITAL 27 // for pulseIn

// Modular bare-metal ultrasonic setup function
void ultraSetup(unsigned char trigger, unsigned char echo, volatile unsigned char *trig_register, volatile unsigned char *echo_register) {
    *trig_register |= trigger;
    *echo_register &= ~(echo);
}

// Partially bare-metal ultrasonic distance function
double getUltraDist(volatile unsigned char *trig_register, int trigger, int echo_digital) {
    *trig_register &= ~(trigger);
    delayMicroseconds(2);
    *trig_register |= (trigger);
    delayMicroseconds(10);
    *trig_register &= ~(trigger);
    long duration = pulseIn(echo_digital, HIGH, 5000);
    return duration * 0.034 / 2;
}

double getUltra(UltraSensor ultra) {
    static bool isSetup = false;
    if (!isSetup) {
        ultraSetup(TRIG_PIN_FRONT, ECHO_PIN_FRONT, &TRIG_PIN_FRONT_DDR, &ECHO_PIN_FRONT_DDR);
        ultraSetup(TRIG_PIN_REAR, ECHO_PIN_REAR, &TRIG_PIN_REAR_DDR, &ECHO_PIN_REAR_DDR);
        isSetup = true;
    }

    if (ultra == FRONT) return getUltraDist(&TRIG_PIN_FRONT_PORT, TRIG_PIN_FRONT, ECHO_FRONT_DIGITAL);
    return getUltraDist(&TRIG_PIN_REAR_PORT, TRIG_PIN_REAR, ECHO_REAR_DIGITAL);
}
