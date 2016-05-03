#include "ringbuffer.h"

#include <assert.h>

// TODO: Modify count correctly when you cross the wrap around/pass the read ptr
// TODO: Modify the read ptr when the write pointer passes it.
//       So if we write 6 bytes into a buffer of length 4 without
//       reading anything, then the read pointer needs to be at 2
//       and not at 0, we don't want to suddenly jump forwards and
//       then have it skip back later when it reaches the write ptr


RingBuffer::RingBuffer(int size)
    : capacity(size)
{
    buffer = new float[size];
    _count = 0;
    readIndex = 0;
    writeIndex = 0;
}

RingBuffer::~RingBuffer()
{
    delete[] buffer;
}

void RingBuffer::write(float val)
{
    buffer[writeIndex] = val;
}

void RingBuffer::write(int valCount, float* vals)
{
    assert(free() >= valCount);

    int contiguousFreeSpace = capacity - writeIndex;
    if(contiguousFreeSpace > valCount)
    {
        for(int i=0; i<valCount; ++i)
            buffer[writeIndex+i] = vals[i];
    }
    else
    {
        for(int i=0; i<contiguousFreeSpace; ++i)
            buffer[writeIndex+i] = vals[i];

        int wrappedValCount = valCount - contiguousFreeSpace;
        for(int i=0; i<wrappedValCount; ++i)
            buffer[i] = vals[contiguousFreeSpace+i];
    }
}

void RingBuffer::advanceWritePointer(int increment)
{
    assert(increment <= capacity);
    writeIndex = (writeIndex+increment)%capacity;
    _count += increment;
}

float RingBuffer::read()
{
    float val = buffer[readIndex];
    return val;
}

void RingBuffer::read(int valCount, float* vals)
{
    int contiguousAvailableValues = capacity - readIndex;
    if(contiguousAvailableValues > valCount)
    {
        for(int i=0; i<valCount; ++i)
        {
            vals[i] = buffer[readIndex+i];
        }
    }
    else
    {
        for(int i=0; i<contiguousAvailableValues; ++i)
        {
            vals[i] = buffer[readIndex+i];
        }

        int wrappedValCount = valCount - contiguousAvailableValues;
        for(int i=0; i<wrappedValCount; ++i)
        {
            vals[contiguousAvailableValues+i] = buffer[i];
        }
    }
}

void RingBuffer::advanceReadPointer(int increment)
{
    readIndex = (readIndex+increment)%capacity;
    _count -= increment;
}

int RingBuffer::count()
{
    return _count;
    int result;
    if(writeIndex >= readIndex)
    {
        result = writeIndex - readIndex;
    }
    else
    {
        result = (capacity - readIndex) + writeIndex;
    }
    return result;
}

int RingBuffer::free()
{
    int result = capacity - count();
    return result;
}
