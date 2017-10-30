/**
 * This is a simple library to control the Boss MS-3.
 *
 * Debug information:
 * - Define MS3_DEBUG_MODE in your sketch including loading this library.
 *
 * Dependencies
 * - https://github.com/felis/USB_Host_Shield_2.0
 *
 * @TODO: split into .h and .cpp.
 * @TODO: add more example sketches.
 * @TODO: write documentation.
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

#include "Arduino.h"
#include "usbh_midi.h"

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
const uint8_t INIT_DELAY_MSEC = 60;
const uint8_t SEND_DELAY_MSEC = 4;
const uint8_t RESPONSE_TIMEOUT_MSEC = 200;
const uint8_t QUEUE_SIZE = 20;

// Return values.
const uint8_t NO_DATA_RECEIVED = 0;
const uint8_t DATA_RECEIVED = 1;
const int8_t MS3_NOT_READY = 0;
const int8_t MS3_READY = 1;
const int8_t MS3_JUST_READY = 2;

// Fixed data.
const uint8_t SYSEX_START = 0xF0;
const uint8_t SYSEX_END = 0xF7;
const uint8_t HEADER[6] = {0x41, 0x00, 0x00, 0x00, 0x00, 0x3B};
const uint8_t HANDSHAKE[15] = {0xF0, 0x7E, 0x00, 0x06, 0x02, 0x41, 0x3B, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
const uint32_t P_EDIT = 0x7F000001;

class MS3 : public USBH_MIDI {
    private:
        USB Usb;
        uint8_t lastState;
        bool ready = false;

        // Queue definitions.
        typedef struct {
            uint32_t address;
            uint8_t data;
            uint8_t dataLength;
            uint8_t operation;
            bool answer;
        } queueData;
        uint32_t lastSend = 0;
        queueData queue[QUEUE_SIZE];
        uint8_t queueReadPointer = 0;
        uint8_t queueWritePointer = 0;

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
         * Strip the protocol data from the incoming data through USB.
         */
        uint8_t extractSysExData(uint8_t *p, uint8_t *buf) {
            uint8_t rc = 0;
            uint8_t cin = *(p) & 0x0f;

            // SysEx message?
            if ((cin & 0xc) != 4) {
                return rc;
            }

            switch(cin) {
                case 4:
                case 7:
                    *buf++ = *(p+1);
                    *buf++ = *(p+2);
                    *buf++ = *(p+3);
                    rc = 3;
                    break;
                case 6:
                    *buf++ = *(p+1);
                    *buf++ = *(p+2);
                    rc = 2;
                    break;
                case 5:
                    *buf++ = *(p+1);
                    rc = 1;
                    break;
                default:
                    break;
            }
            return(rc);
        }

        /**
         * Construct a full message.
         */
        void send(const uint32_t address, uint8_t *data, uint8_t dataLength, uint8_t action) {
            uint8_t sysex[14 + dataLength] = {0};

            memcpy(sysex + 1, HEADER, 7);
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

            MS3_DEBUG(F("TX:")); MS3::printSysEx(data, dataLength);
            if ((result = MS3::SendSysEx(data, dataLength)) != 0) {
                MS3_DEBUG(F(" *** Transfer error: ")); MS3_DEBUG(result);
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
            MS3_DEBUG(F(" (")); MS3_DEBUG(dataLength); MS3_DEBUG(F(")"));
        }

        /**
         * Check if we've received any data.
         *
         * @TODO: support different data lengths.
         */
        uint8_t receive(uint32_t &parameter, uint8_t *dataOut) {
            uint8_t
                incoming[MIDI_EVENT_PACKET_SIZE] = {0},
                data[MIDI_EVENT_PACKET_SIZE] = {0},
                dataLength = 0,
                i;

            uint16_t rcvd;

            if (MS3::RecvData(&rcvd, incoming) == 0) {
                if (rcvd == 0) {
                    return NO_DATA_RECEIVED;
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
                MS3_DEBUG(F("RX:")); MS3::printSysEx(data, dataLength); MS3_DEBUGLN();

                // Return values.
                parameter = 0;
                for (i = 0; i < 4; i++) {
                    parameter += (uint32_t)data[8 + i] << (3 - i) * 8;
                }
                dataOut[0] = data[dataLength - 3];

                return DATA_RECEIVED;
            }

            return NO_DATA_RECEIVED;
        }
    public:

        /**
         * Constructor.
         */
        MS3() : USBH_MIDI(&Usb) {
            if (Usb.Init() == -1) {
                MS3_DEBUGLN(F("*** USB Init error"));
                while (1);
            }
        }

        /**
         * Check if the USB layer is ready, and optionally initialize the MS-3
         * and set it to Editor mode.
         */
        uint8_t isReady() {
            Usb.Task();
            if (Usb.getUsbTaskState() == USB_STATE_RUNNING) {
                if (!MS3::ready) {

                    // Init the editor mode.
                    MS3::send((uint8_t *)HANDSHAKE);
                    delay(SEND_DELAY_MSEC);
                    MS3::send((uint8_t *)HANDSHAKE);
                    delay(SEND_DELAY_MSEC);
                    uint8_t data[1] = {0x01};
                    MS3::send(P_EDIT, data, 1, 0x12);
                    delay(INIT_DELAY_MSEC);

                    MS3::ready = true;
                    MS3_DEBUGLN(F("*** Up and ready!"));

                    return MS3_JUST_READY;
                }

                return MS3_READY;
            }
            else if (MS3::lastState != Usb.getUsbTaskState()) {
                MS3_DEBUG(F("*** USB task state: ")); MS3_DEBUG_AS(MS3::lastState = Usb.getUsbTaskState(), HEX); MS3_DEBUGLN();
                MS3::ready = false;
            }

            return MS3_NOT_READY;
        }

        int8_t handleQueue(uint32_t &parameter, uint8_t *dataOut) {
            if (queueReadPointer != queueWritePointer && lastSend + SEND_DELAY_MSEC < millis()) {
                int8_t reponse = NO_DATA_RECEIVED;
                lastSend = millis();

                MS3_DEBUG(F("Handle queue: ")); MS3_DEBUGLN(queueReadPointer);

                uint8_t input[queue[queueReadPointer].dataLength] = {0};
                input[queue[queueReadPointer].dataLength - 1] = queue[queueReadPointer].data;

                MS3::send(
                    queue[queueReadPointer].address,
                    input,
                    queue[queueReadPointer].dataLength,
                    queue[queueReadPointer].operation
                );

                if (queue[queueReadPointer].answer) {
                    while ((reponse = MS3::receive(parameter, dataOut)) == NO_DATA_RECEIVED && lastSend + RESPONSE_TIMEOUT_MSEC > millis()) {
                        MS3_DEBUGLN(F("*** Waiting for an answer"));
                    }
                    if (reponse == NO_DATA_RECEIVED) {
                        MS3_DEBUGLN(F("*** Time-out reached."));
                    }
                }

                queueReadPointer = (queueReadPointer < QUEUE_SIZE - 1) ? queueReadPointer + 1 : 0;

                return reponse;
            }

            // If the queue is empty, check for incoming data.
            queueReadPointer = queueWritePointer = 0;
            return MS3::receive(parameter, dataOut);
        }

        /**
         * Set this single byte parameter on the MS-3.
         */
        void set(const uint32_t address, uint8_t data) {
            MS3::set(address, data, 1);
        }
        void set(const uint32_t address, uint8_t data, uint8_t dataLength) {
            MS3_DEBUG(F("Add to queue: ")); MS3_DEBUGLN(queueWritePointer);

            queue[queueWritePointer].address = address;
            queue[queueWritePointer].data = data;
            queue[queueWritePointer].dataLength = dataLength;
            queue[queueWritePointer].operation = 0x12;
            queue[queueWritePointer].answer = false;

            queueWritePointer = (queueWritePointer < QUEUE_SIZE - 1) ? queueWritePointer + 1 : 0;
        }

        /**
         * Tell the MS-3 to send us the value of this paramater.
         *
         * @see MS3::receive()
         */
        void get(const uint32_t address, uint8_t data) {
            MS3_DEBUG(F("Add to queue: ")); MS3_DEBUGLN(queueWritePointer);

            queue[queueWritePointer].address = address;
            queue[queueWritePointer].data = data;
            queue[queueWritePointer].dataLength = 4;
            queue[queueWritePointer].operation = 0x11;
            queue[queueWritePointer].answer = true;

            queueWritePointer = (queueWritePointer < QUEUE_SIZE - 1) ? queueWritePointer + 1 : 0;
        }

        /**
         * Check if the queue is currently empty.
         */
        bool queueIsEmpty() {
            return queueReadPointer == queueWritePointer;
        }
};

#endif
