#include "ringbuffer.h"

#include <assert.h>
#include <atomic>

#include "platform.h"
#include "logging.h"

RingBuffer::RingBuffer(int size)
    : capacity(size), readIndex(0), writeIndex(0)
{
    buffer = new float[size];
    lock = createMutex();
}

RingBuffer::~RingBuffer()
{
    destroyMutex(lock);
    delete[] buffer;
}

void RingBuffer::write(int valCount, float* vals)
{
    lockMutex(lock);
    assert(valCount < capacity);

    if(valCount > free())
    {
        int readIndexIncrement = valCount - free();
        readIndex += readIndexIncrement;
        if(readIndex >= capacity)
        {
            readIndex -= capacity;
        }
    }

    int contiguousFreeSpace = capacity - writeIndex;
    if(contiguousFreeSpace > valCount)
    {
        for(int i=0; i<valCount; ++i)
        {
            buffer[writeIndex+i] = vals[i];
        }
    }
    else
    {
        for(int i=0; i<contiguousFreeSpace; ++i)
        {
            buffer[writeIndex+i] = vals[i];
        }

        int wrappedValCount = valCount - contiguousFreeSpace;
        for(int i=0; i<wrappedValCount; ++i)
        {
            buffer[i] = vals[contiguousFreeSpace+i];
        }
    }

    writeIndex += valCount;
    if(writeIndex >= capacity)
    {
        writeIndex -= capacity;
    }
    unlockMutex(lock);
}

void RingBuffer::read(int valCount, float* vals)
{
    lockMutex(lock);
    assert(valCount < capacity);

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

    readIndex += valCount;
    if(readIndex >= capacity)
    {
        readIndex -= capacity;
    }
    unlockMutex(lock);
}

int RingBuffer::count()
{
    // NOTE: The order is important here, if we read the writeIndex second, we might get
    //       a count that is larger than it really is. This is bad but we're ok with getting one
    //       that is smaller (since we'll just miss a couple values instead of getting wrong ones)
    lockMutex(lock);
    int localWriteIndex = writeIndex;
    int localReadIndex = readIndex;
    unlockMutex(lock);

    if(localWriteIndex < localReadIndex)
        return (capacity - localReadIndex) + localWriteIndex;
    else
        return localWriteIndex - localReadIndex;
}

int RingBuffer::free()
{
    // NOTE: As with count(), order is important here. We load read first then write so that we
    //       err on the side of giving a smaller-than-correct value
    lockMutex(lock);
    int localReadIndex = readIndex;
    int localWriteIndex = writeIndex;
    unlockMutex(lock);

    int numberOfSlots;
    if(localReadIndex <= localWriteIndex)
        numberOfSlots = (capacity - localWriteIndex) + localReadIndex;
    else
        numberOfSlots = localReadIndex - localWriteIndex;

    // NOTE: We subtract 1 here so that readIndex == writeIndex only if the buffer is empty
    return numberOfSlots - 1;
}

void RingBuffer::clear()
{
    lockMutex(lock);
    writeIndex = 0;
    readIndex = 0;
    unlockMutex(lock);
}
