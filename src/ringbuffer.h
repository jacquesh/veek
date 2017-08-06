#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include <atomic>

#include "platform.h"

class RingBuffer
{
public:
    RingBuffer(int size);
    ~RingBuffer();

    // Write an array of values into the buffer
    void write(int valCount, float* vals);

    // Read an array of values out of the buffer, writing them into the given vals array
    void read(int valCount, float* vals);

    // Returns the number of items that are available for reading in the buffer
    int count();

    // Returns the number of items that can be written without reaching the read pointer.
    // If we write exactly this number of items, then the write pointer will be immediately
    // behind the read pointer.
    // The read pointer will be moved forward if we write free()+1 elements.
    int free();

    // Empty the ringbuffer
    void clear();

private:
    int capacity;
    float* buffer;

    int readIndex;
    int writeIndex;
    Mutex* lock;
};

#endif
