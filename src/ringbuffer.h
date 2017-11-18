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

    // Write an array of values into the buffer
    void write(int valCount, float* vals);

    // Read an array of values out of the buffer, writing them into the given vals array
    //
    // Returns the number of values that were written.
    // If there are valCount values available, then valCount will be returned.
    // Otherwise if there are n<valCount values available, n will be returned and the values
    // will be written into vals[0] to vals[n-1].
    // vals[n] to vals[valCount-1] will not be modified.
    int read(int valCount, float* vals);

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

    uint64_t totalWrites;
    uint64_t totalReads;

    // NOTE: These functions are not thread-safe
    int freeInternal();
};

#endif
