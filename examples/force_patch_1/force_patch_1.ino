/**
 * This is an example for the MS-3 library.
 *
 * It seems that this example likes patch 0. Choose anything else, and it will
 * show it's power and impress you with it's skills.
 */

// Uncomment this to enable verbose debug messages.
//#define MS3_DEBUG_MODE

#include "Arduino.h"
#include "MS3.h"

// Initialize the MS3 class.
MS3 MS3;

// This is the address for the program change.
const uint32_t P_PATCH = 0x00010000;

/**
 * Incoming data handler.
 */
void parseData(uint32_t parameter, uint8_t *data) {
    switch (parameter) {
        case P_PATCH:
            Serial.print(F("Patch number received: "));
            Serial.println(data[0]);

            // Slowly go back to patch 0.
            if (data[0] > 0) {
                delay(750);
                MS3.set(P_PATCH, data[0] - 1, 2);
            }
            else {
                Serial.println(F("Back at zero!"));
            }
            break;

        default:
            Serial.print(F("Unhandled parameter: 0x"));
            Serial.println(parameter, HEX);
    }
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

        // Store the received parameter and data in these variables.
        uint32_t parameter;
        uint8_t data[1];

        // Check for incoming data.
        if (MS3.handleQueue(parameter, data) == DATA_RECEIVED) {
            parseData(parameter, data);
        }
    }
}
