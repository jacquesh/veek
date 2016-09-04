#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include <atomic>

#include "platform.h"

// TODO: Write a copy of this which is efficiently thread-safe, the current implementation just wraps everything in a mutex acquire...which is meh
/*
 * NOTE: This ring buffer implementation is not thread-safe, you should do your own mutual
 *       exclusion if you're using this from multiple threads
 */
class RingBuffer
{
public:
    // size must be a power of 2
    RingBuffer(int size);
    ~RingBuffer();

    // Write an array of values into the buffer
    void write(int valCount, float* vals);

    // Read an array of values out of the buffer, writing them into the given vals array
    void read(int valCount, float* vals);

    // Returns the number of items that are available for reading in the buffer
    int count(); // TODO: This needs to be thread-safe

    // Returns the number of items that can be written without reaching the read pointer.
    // If we write exactly this number of items, then the write pointer will be immediately
    // behind the read pointer. We will fail if we try to write free()+1 elements.
    int free();

    // Empty the ringbuffer
    void clear();

private:
    int capacity;
    int capacityMask;
    float* buffer;

    std::atomic<int> readIndex;
    std::atomic<int> writeIndex;
};

#endif
