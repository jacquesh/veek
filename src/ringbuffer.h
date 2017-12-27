#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include "platform.h"

// TODO: We might only ever read/write values one at a time, in which case we can greatly simplify
//       the read and write methods that currently handle the potential wrapping logic.
class RingBuffer
{
public:
    RingBuffer(int sampleRate, int size);
    ~RingBuffer();

    // Write a value into the buffer
    // NOTE: If the buffer is full, the oldest value will be removed to make space for the new one.
    void write(float value);

    // Read an array of values out of the buffer, writing them into the given vals array
    //
    // Returns the number of values that were written.
    // If there is a value available, then 1 will be returned and *value will be the resulting value.
    // Otherwise, 0 will be returned and the contents of value will not be modified.
    int read(float* value);

    // Returns the number of items that are available for reading in the buffer
    int count();

    // Returns the number of items that can be written without reaching the read pointer.
    // If we write exactly this number of items, then the write pointer will be immediately
    // behind the read pointer.
    // The read pointer will be moved forward if we write free()+1 elements.
    int free();

    // Empty the ringbuffer
    void clear();


    int sampleRate;
private:
    int capacity;
    float* buffer;

    int readIndex;
    int writeIndex;
    Platform::Mutex* lock;

    // NOTE: These functions are not thread-safe
    int freeInternal();
};

#endif
