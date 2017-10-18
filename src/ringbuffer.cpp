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

    lock = Platform::CreateMutex();

    totalWrites = 0;
    totalReads = 0;
}

RingBuffer::~RingBuffer()
{
    Platform::DestroyMutex(lock);
    delete[] buffer;
}

void RingBuffer::write(int valCount, float* vals)
{
    Platform::LockMutex(lock);
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
    Platform::UnlockMutex(lock);
}

int RingBuffer::read(int valCount, float* vals)
{
    assert(valCount < capacity);
    Platform::LockMutex(lock);

    int contiguousAvailableValues = capacity - readIndex;
    int valuesToRead = valCount;
    if(contiguousAvailableValues > valCount)
    {
        if((writeIndex >= readIndex) && (writeIndex < readIndex+valCount))
        {
            valuesToRead = writeIndex - readIndex;
        }

        for(int i=0; i<valuesToRead; i++)
        {
            vals[i] = buffer[readIndex+i];
        }

        readIndex += valuesToRead;
    }
    else
    {
        // NOTE: This condition is sufficient because it is always true that (writeIndex < capacity)
        if(writeIndex >= readIndex)
        {
            valuesToRead = writeIndex - readIndex;
            contiguousAvailableValues = valuesToRead;
        }

        for(int i=0; i<contiguousAvailableValues; ++i)
        {
            vals[i] = buffer[readIndex+i];
        }

        int wrappedValCount = valuesToRead - contiguousAvailableValues;
        if((wrappedValCount > 0) && (writeIndex < wrappedValCount))
        {
            wrappedValCount = writeIndex;
            valuesToRead = contiguousAvailableValues + wrappedValCount;
        }
        for(int i=0; i<wrappedValCount; ++i)
        {
            vals[contiguousAvailableValues+i] = buffer[i];
        }

        readIndex = wrappedValCount;
    }

    totalReads += valuesToRead;
    Platform::UnlockMutex(lock);

    return valuesToRead;
}

int RingBuffer::count()
{
    Platform::LockMutex(lock);
    int localWriteIndex = writeIndex;
    int localReadIndex = readIndex;
    Platform::UnlockMutex(lock);

    if(localWriteIndex < localReadIndex)
        return (capacity - localReadIndex) + localWriteIndex;
    else
        return localWriteIndex - localReadIndex;
}

int RingBuffer::free()
{
    Platform::LockMutex(lock);
    int result = freeInternal();
    Platform::UnlockMutex(lock);

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
    Platform::LockMutex(lock);
    writeIndex = 0;
    readIndex = 0;
    Platform::UnlockMutex(lock);
}
