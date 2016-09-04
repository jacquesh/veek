#include "ringbuffer.h"

#include <assert.h>
#include <atomic>

#include "platform.h"
#include "logging.h"

RingBuffer::RingBuffer(int size)
    : capacity(size), capacityMask(size-1), readIndex(0), writeIndex(0)
{
    assert(size && !(size & (size-1))); // We require power-of-2 capacity so that we can use
                                        // atomic AND operations instead of a modulo
    buffer = new float[size];
}

RingBuffer::~RingBuffer()
{
    delete[] buffer;
}

void RingBuffer::write(int valCount, float* vals)
{
    // NOTE: We must be strictly less than capacity so that our pointers don't coincide afterwards
    assert(valCount < capacity);

    if(valCount > free())
    {
        int readIndexIncrement = valCount - free();
        readIndex.fetch_add(readIndexIncrement);
        readIndex.fetch_and(capacityMask);
        // TODO: We should probably check that this is sufficient, can we not ask to read a large
        //       block of data, in which case we will start reading, then get here and write through
        //       the area that we're currently reading, which would cause a discontinuity
        //
        //       So we might need a mutex here or something...
    }

    int localWriteIndex = writeIndex.load();
    int contiguousFreeSpace = capacity - localWriteIndex;
    if(contiguousFreeSpace > valCount)
    {
        for(int i=0; i<valCount; ++i)
            buffer[localWriteIndex+i] = vals[i];
    }
    else
    {
        for(int i=0; i<contiguousFreeSpace; ++i)
            buffer[localWriteIndex+i] = vals[i];

        int wrappedValCount = valCount - contiguousFreeSpace;
        for(int i=0; i<wrappedValCount; ++i)
            buffer[i] = vals[contiguousFreeSpace+i];
    }

    writeIndex.fetch_add(valCount);
    // NOTE: See corresponding note in RingBuffer::read
    writeIndex.fetch_and(capacityMask);
}

void RingBuffer::read(int valCount, float* vals)
{
    assert(valCount < capacity);

    int localReadIndex = readIndex.load();
    int contiguousAvailableValues = capacity - localReadIndex;
    if(contiguousAvailableValues > valCount)
    {
        for(int i=0; i<valCount; ++i)
        {
            vals[i] = buffer[localReadIndex+i];
        }
    }
    else
    {
        for(int i=0; i<contiguousAvailableValues; ++i)
        {
            vals[i] = buffer[localReadIndex+i];
        }

        int wrappedValCount = valCount - contiguousAvailableValues;
        for(int i=0; i<wrappedValCount; ++i)
        {
            vals[contiguousAvailableValues+i] = buffer[i];
        }
    }

    readIndex.fetch_add(valCount);
    // NOTE: We need to wrap if we go past capacity, so we AND.
    //       This only works because we require power-of-2 capacity,
    //       also because AND is idempotent so it doesn't matter if we interleave here
    readIndex.fetch_and(capacityMask);
}

int RingBuffer::count()
{
    // NOTE: The order is important here, if we read the writeIndex second, we might get
    //       a count that is larger than it really is. This is bad but we're ok with getting one
    //       that is smaller (since we'll just miss a couple values instead of getting wrong ones)
    int localWriteIndex = writeIndex.load();
    int localReadIndex = readIndex.load();

    if(localWriteIndex < localReadIndex)
        return (capacity - localReadIndex) + localWriteIndex;
    else
        return localWriteIndex - localReadIndex;
}

int RingBuffer::free()
{
    // NOTE: As with count(), order is important here. We load read first then write so that we
    //       err on the side of giving a smaller-than-correct value
    int localReadIndex = readIndex.load();
    int localWriteIndex = writeIndex.load();

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
    writeIndex.store(0);
    readIndex.store(0);
}
