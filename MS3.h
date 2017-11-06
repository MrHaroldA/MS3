/**
 * This is a simple library to control the Boss MS-3.
 *
 * Check README.md or visit https://github.com/MrHaroldA/MS3 for more information.
 *
 * Debug information:
 * - Define MS3_DEBUG_MODE in your sketch before including this library.
 *
 * Dependencies
 * - An Arduino with a USB Host Shield
 * - https://github.com/felis/USB_Host_Shield_2.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MS3_h
#define MS3_h

/**
 * Overridable config.
 *
 * MS3_WRITE_INTERVAL_MSEC: The delay before a new message is sent after a write action.
 * MS3_READ_INTERVAL_MSEC: The delay before a new message is sent after a read action.
 * MS3_RECEIVE_INTERVAL_MSEC: The delay after receiving data from the MS-3.
 * MS3_HEADER: The manufacturer and device id header.
 * MS3_QUEUE_SIZE: The maximum number of items in the send queue.
 */
#ifndef MS3_WRITE_INTERVAL_MSEC
const uint8_t MS3_WRITE_INTERVAL_MSEC = 0;
#endif

#ifndef MS3_READ_INTERVAL_MSEC
const uint8_t MS3_READ_INTERVAL_MSEC = 25;
#endif

#ifndef MS3_RECEIVE_INTERVAL_MSEC
const uint8_t MS3_RECEIVE_INTERVAL_MSEC = 0;
#endif

#ifndef MS3_HEADER
const uint8_t MS3_HEADER[6] = {0x41, 0x00, 0x00, 0x00, 0x00, 0x3B};
#endif

#ifndef MS3_QUEUE_SIZE
const uint8_t MS3_QUEUE_SIZE = 20;
#endif

/**
 * The configuration options below are internal and should not be changed.
 */
#include "Arduino.h"
#include "usbh_midi.h"
#include "Queue.h"

#ifndef MS3_DEBUG
    #ifdef MS3_DEBUG_MODE
        #define MS3_DEBUG(x) Serial.print(x)
        #define MS3_DEBUGLN(x) Serial.println(x)
        #define MS3_DEBUG_AS(x, y) Serial.print(x, y)
    #else
        #define MS3_DEBUG(x)
        #define MS3_DEBUGLN(x)
        #define MS3_DEBUG_AS(x, y)
    #endif
#endif

// General configuration.
const uint16_t INIT_DELAY_MSEC = 60;
const uint8_t MS3_WRITE = 0x12;
const uint8_t MS3_READ = 0x11;

// Return values.
const int8_t MS3_NOT_READY = 0;
const int8_t MS3_READY = 1;
const int8_t MS3_JUST_READY = 2;

// Fixed data.
const uint8_t SYSEX_START = 0xF0;
const uint8_t SYSEX_END = 0xF7;
const uint8_t HANDSHAKE[15] = {0xF0, 0x7E, 0x00, 0x06, 0x02, 0x41, 0x3B, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
const uint32_t P_EDIT = 0x7F000001;

// Load the Queue class.
Queue Queue;

class MS3 : public USBH_MIDI {
    private:
        USB Usb;
        uint8_t lastState = 0;
        bool ready = false;
        uint32_t nextMessage = 0;

        /**
         * The last bit of the data sent to the MS-3 contains a checkum of the parameter and data.
         */
        uint8_t checksum(uint8_t *data, uint8_t dataLength) {
            uint8_t sum = 0, i;

            for (i = 8; i < 12 + dataLength; i++) {
                sum = (sum + data[i]) & 0x7F;
            }

            return (128 - sum) & 0x7F;
        }

        /**
         * Construct a full message.
         */
        void send(const uint32_t address, uint8_t *data, uint8_t dataLength, uint8_t action) {
            uint8_t sysex[14 + dataLength] = {0};

            memcpy(sysex + 1, MS3_HEADER, 7);
            sysex[8] = address >> 24;
            sysex[9] = address >> 16;
            sysex[10] = address >> 8;
            sysex[11] = address;
            memcpy(sysex + 12, data, dataLength);

            sysex[0] = SYSEX_START;
            sysex[7] = action;
            sysex[12 + dataLength] = checksum(sysex, dataLength);
            sysex[13 + dataLength] = SYSEX_END;

            MS3::send(sysex);
        }

        /**
         * Send the data to the MS-3.
         */
        void send(uint8_t *data) {
            uint8_t result, dataLength = MS3::countSysExDataSize(data);

            MS3_DEBUG(F("TX:"));
            MS3::printSysEx(data, dataLength);
            if ((result = MS3::SendSysEx(data, dataLength)) != 0) {
                MS3_DEBUG(F(" *** Transfer error: "));
                MS3_DEBUG(result);
            }
            MS3_DEBUGLN();
        }

        /**
         * MS3_DEBUG printer for the received SysEx data.
         */
        void printSysEx(uint8_t *data, uint8_t dataLength) {
            char buf[10];

            for (uint8_t i = 0; i < dataLength; i++) {
                sprintf(buf, " %02X", data[i]);
                MS3_DEBUG(buf);
            }
            MS3_DEBUG(F(" ("));
            MS3_DEBUG(dataLength);
            MS3_DEBUG(F(")"));
        }

        /**
         * Check if we've received any data.
         *
         * @TODO: support different data lengths.
         */
        bool receive(uint32_t &parameter, uint8_t *dataOut) {
            uint8_t
                incoming[MIDI_EVENT_PACKET_SIZE] = {0},
                data[MIDI_EVENT_PACKET_SIZE] = {0},
                dataLength = 0,
                i;

            uint16_t rcvd;

            if (MS3::RecvData(&rcvd, incoming) == 0) {
                if (rcvd == 0) {
                    return false;
                }

                uint8_t *p = incoming;
                for (i = 0; i < MIDI_EVENT_PACKET_SIZE; i += 4) {
                    if (*p == 0 && *(p + 1) == 0) {
                        break;
                    }

                    uint8_t chunk[3] = {0};
                    if (MS3::extractSysExData(p, chunk) != 0) {
                        for (uint8_t j = 0; j < 3; j++) {
                            data[dataLength] = chunk[j];
                            dataLength++;

                            if (chunk[j] == SYSEX_END) {
                                break;
                            }
                        }
                        p += 4;
                    }
                }
                MS3_DEBUG(F("RX:"));
                MS3::printSysEx(data, dataLength);
                MS3_DEBUGLN();

                // Return values.
                parameter = 0;
                for (i = 0; i < 4; i++) {
                    parameter += (uint32_t) data[8 + i] << (3 - i) * 8;
                }
                dataOut[0] = data[dataLength - 3];

                return true;
            }

            return false;
        }

    public:

        /**
         * Constructor.
         */
        MS3() : USBH_MIDI(&Usb) {
            if (Usb.Init() == -1) {
                MS3_DEBUGLN(F("*** USB Init error"));
                while (true);
            }
        }

        /**
         * Init the editor mode.
         */
        void begin() {
            MS3::send((uint8_t *) HANDSHAKE);
            delay(MS3_WRITE_INTERVAL_MSEC);
            MS3::send((uint8_t *) HANDSHAKE);
            delay(MS3_WRITE_INTERVAL_MSEC);
            uint8_t data[1] = {0x01};
            MS3::send(P_EDIT, data, 1, MS3_WRITE);
            delay(INIT_DELAY_MSEC);
            MS3_DEBUGLN(F("*** Up and ready!"));
        }

        /**
         * Check if the USB layer is ready, and optionally initialize the MS-3
         * and set it to Editor mode.
         */
        uint8_t isReady() {
            Usb.Task();
            if (Usb.getUsbTaskState() == USB_STATE_RUNNING) {
                if (!MS3::ready) {
                    MS3::ready = true;
                    return MS3_JUST_READY;
                }

                return MS3_READY;
            }
            else if (MS3::lastState != Usb.getUsbTaskState()) {
                MS3_DEBUG(F("*** USB task state: "));
                MS3_DEBUG_AS(MS3::lastState = Usb.getUsbTaskState(), HEX);
                MS3_DEBUGLN();
                MS3::ready = false;
            }

            return MS3_NOT_READY;
        }

        /**
         * This is the main function for both receiving and sending data when
         * there's nothing to receive.
         */
        bool update(uint32_t &parameter, uint8_t *data) {

            // Is there data waiting to be picked up?
            if (MS3::receive(parameter, data)) {
                MS3::nextMessage = millis() + MS3_RECEIVE_INTERVAL_MSEC;
                return true;
            }

            // Check if we need to send out a queued item.
            queueItem item = {};
            if (MS3::nextMessage <= millis() && Queue.read(item)) {
                bool reponse = false;

                // Construct the data to send to the MS-3.
                uint8_t input[item.dataLength] = {0};
                input[item.dataLength - 1] = item.data;

                // Send the queue item to the MS-3.
                MS3::send(
                    item.address,
                    input,
                    item.dataLength,
                    item.operation
                );

                MS3::nextMessage = millis() + (item.operation == MS3_READ ? MS3_READ_INTERVAL_MSEC : MS3_WRITE_INTERVAL_MSEC);
                return reponse;
            }

            // Nothing happened.
            return false;
        }

        /**
         * Set this single byte parameter on the MS-3.
         */
        void write(const uint32_t address, uint8_t data) {
            Queue.write(address, data, 1, MS3_WRITE);
        }

        void write(const uint32_t address, uint8_t data, uint8_t dataLength) {
            Queue.write(address, data, dataLength, MS3_WRITE);
        }

        /**
         * Tell the MS-3 to send us the value of this paramater.
         */
        void read(const uint32_t address, uint8_t data) {
            Queue.write(address, data, 4, MS3_READ);
        }

        /**
         * Return if the queue is currently empty.
         */
        bool queueIsEmpty() {
            return Queue.isEmpty();
        }
};

#endif
