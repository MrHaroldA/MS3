#ifndef MS3_Queue_h
#define MS3_Queue_h

#include "Arduino.h"
#include "MS3.h"

typedef struct {
    uint32_t address;
    uint8_t data;
    uint8_t dataLength;
    uint8_t operation;
    bool answer;
} queueItem;

class Queue {
    private:
        queueItem queue[QUEUE_SIZE];
        uint8_t readPointer = 0;
        uint8_t writePointer = 0;

    public:

        /**
         * Get the current queue item. This always returns an item, even when the queue is considered empty.
         */
        bool get(queueItem &item) {
            if (Queue::isEmpty()) {
                return false;
            }

            MS3_DEBUG(F("Handle queue: ")); MS3_DEBUGLN(readPointer);

            item = queue[readPointer];

            readPointer = (readPointer < QUEUE_SIZE - 1) ? readPointer + 1 : 0;

            return true;
        }

        /**
         * Add an item to the queue.
         *
         * @TODO: add protection against overwriting items.
         */
        void set(uint32_t address, uint8_t data, uint8_t dataLength, uint8_t operation, bool answer) {
            MS3_DEBUG(F("Add to queue: ")); MS3_DEBUGLN(writePointer);

            queue[writePointer].address = address;
            queue[writePointer].data = data;
            queue[writePointer].dataLength = dataLength;
            queue[writePointer].operation = operation;
            queue[writePointer].answer = answer;

            writePointer = (writePointer < QUEUE_SIZE - 1) ? writePointer + 1 : 0;
        }

        /**
         * Check if the queue is currently empty.
         */
        bool isEmpty() {
            if (readPointer == writePointer) {
                readPointer = writePointer = 0;
                return true;
            }

            return false;
        }
};

#endif
