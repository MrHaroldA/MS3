/**
 * This is an example for the MS-3 library.
 *
 * What have you done? Let's find out what's happening with all effect blocks.
 */

// Uncomment this to enable verbose debug messages.
//#define MS3_DEBUG_MODE

#include "Arduino.h"
#include "MS3.h"

// Initialize the MS3 class.
MS3 MS3;

// These are adresses for the effect states.
const uint32_t P_FX1   = 0x60000030;
const uint32_t P_MOD1  = 0x60000430;
const uint32_t P_L1    = 0x60000020;
const uint32_t P_L2    = 0x60000021;
const uint32_t P_L3    = 0x60000022;
const uint32_t P_FX2   = 0x60000229;
const uint32_t P_MOD2  = 0x6000051E;
const uint32_t P_DLY   = 0x60000610;
const uint32_t P_REV   = 0x60000630;
const uint32_t P_NS    = 0x60000656;
const uint32_t P_SOLO  = 0x60000B02;
const uint32_t P_CTL1  = 0x60000B05;
const uint32_t P_CTL2  = 0x60000B06;

// This is the adress for the program change.
const uint32_t P_PATCH = 0x00010000;

// We're going to check and report these effect blocks.
const uint32_t CHECK_THIS[] = {
    P_FX1,
    P_MOD1,
    P_L1,
    P_L2,
    P_L3,
    P_FX2,
    P_MOD2,
    P_DLY,
    P_REV,
    P_NS,
    P_SOLO,
    P_CTL1,
    P_CTL2
};
const uint8_t CHECK_THIS_SIZE = sizeof(CHECK_THIS) / sizeof(CHECK_THIS[0]);

// Some global variables to store effect state and changes.
uint16_t states = 0;
bool changed = false;

/**
 * Incoming data handler.
 */
void parseData(uint32_t parameter, uint8_t *data) {
    switch (parameter) {

        // Refresh all effect states on patch changes.
        case P_PATCH:
            for (uint8_t i = 0; i < CHECK_THIS_SIZE; i++) {
                MS3.get(CHECK_THIS[i], 0x01);
            }
            break;

        // Store all effect states for printing later.
        default:
            for (uint8_t i = 0; i < CHECK_THIS_SIZE; i++) {
                if (CHECK_THIS[i] == parameter) {
                    bitWrite(states, i, data[0]);

                    // Mark as changed.
                    changed = true;
                }
            }
    }
}

/**
 * Print all effect states.
 */
void printStatus() {
    Serial.println();

    char state[4];
    for (uint8_t i = 0; i < CHECK_THIS_SIZE; i++) {
        strcpy(state, (bitRead(states, i) ? "ON" : "OFF"));

        switch (CHECK_THIS[i]) {
            case P_FX1:  Serial.print(F("FX1:  ")); Serial.println(state); break;
            case P_MOD1: Serial.print(F("MOD1: ")); Serial.println(state); break;
            case P_L1:   Serial.print(F("L1:   ")); Serial.println(state); break;
            case P_L2:   Serial.print(F("L2:   ")); Serial.println(state); break;
            case P_L3:   Serial.print(F("L3:   ")); Serial.println(state); break;
            case P_FX2:  Serial.print(F("FX2:  ")); Serial.println(state); break;
            case P_MOD2: Serial.print(F("MOD2: ")); Serial.println(state); break;
            case P_DLY:  Serial.print(F("DLY:  ")); Serial.println(state); break;
            case P_REV:  Serial.print(F("REV:  ")); Serial.println(state); break;
            case P_NS:   Serial.print(F("NS:   ")); Serial.println(state); break;
            case P_SOLO: Serial.print(F("SOLO: ")); Serial.println(state); break;
            case P_CTL1: Serial.print(F("CTL1: ")); Serial.println(state); break;
            case P_CTL2: Serial.print(F("CTL2: ")); Serial.println(state); break;

            default: Serial.print(CHECK_THIS[i]); Serial.print(F(": ")); Serial.println(state);
        }
    }

    Serial.println();
}

/**
 * Setup routine.
 */
void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println(F("Ready!")); Serial.println();
}

/**
 * Main loop.
 */
void loop() {
    int8_t state;

    // Check if the MS-3 is listening.
    if ((state = MS3.isReady()) != MS3_NOT_READY) {

        // Fetch the current active patch on the MS-3.
        if (state == MS3_JUST_READY) {
            MS3.get(P_PATCH, 0x02);
        }

        // The MS-3 library stores the parameter and data in these variables.
        uint32_t parameter;
        uint8_t data[1];

        // Check for incoming data.
        if (MS3.handleQueue(parameter, data) == DATA_RECEIVED) {
            parseData(parameter, data);
        }
    }

    // Only print the states if something changed, and the queue is empty.
    if (changed && MS3.queueIsEmpty()) {
        printStatus();
        changed = false;
    }
}