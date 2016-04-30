#include "ringbuffer.h"

#include <assert.h>

RingBuffer::RingBuffer(int size)
    : capacity(size)
{
    buffer = new float[size];
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
}

int RingBuffer::available()
{
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
    int result = capacity - available();
    return result;
}
