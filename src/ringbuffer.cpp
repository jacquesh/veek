#include <assert.h>
#include <string.h>

#include "platform.h"
#include "logging.h"
#include "ringbuffer.h"

RingBuffer::RingBuffer(int size)
    : capacity(size), readIndex(0), writeIndex(0)
{
    buffer = new float[size];

    // TODO: Note that this memset masks an error whereby we read before writing when we
    //       start listening to input (on the listenBuffer).
    //       We can reliably reproduce the issue by simply setting everything to some very large
    //       value (10e10) and looking at the output wave.
    memset(buffer, 0, sizeof(float)*size);

    lock = createMutex();

    totalWrites = 0;
    totalReads = 0;
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

    if(valCount > freeInternal())
    {
        int readIndexIncrement = valCount - freeInternal();
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
    totalWrites += valCount;
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
            if(readIndex+i == writeIndex)
            {
                logTerm("ERROR: Reading past the write pointer @ %d\n", writeIndex);
            }
            vals[i] = buffer[readIndex+i];
        }
    }
    else
    {
        for(int i=0; i<contiguousAvailableValues; ++i)
        {
            if(readIndex+i == writeIndex)
            {
                logTerm("ERROR: Reading past the write pointer @ %d\n", writeIndex);
            }
            vals[i] = buffer[readIndex+i];
        }

        int wrappedValCount = valCount - contiguousAvailableValues;
        for(int i=0; i<wrappedValCount; ++i)
        {
            if(i == writeIndex)
            {
                logTerm("ERROR: Reading past the write pointer @ %d\n", writeIndex);
            }
            vals[contiguousAvailableValues+i] = buffer[i];
        }
    }

    readIndex += valCount;
    if(readIndex >= capacity)
    {
        readIndex -= capacity;
    }
    totalReads += valCount;
    unlockMutex(lock);
}

int RingBuffer::count()
{
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
    lockMutex(lock);
    int result = freeInternal();
    unlockMutex(lock);

    return result;
}

int RingBuffer::freeInternal()
{
    int localReadIndex = readIndex;
    int localWriteIndex = writeIndex;

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
