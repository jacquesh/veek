#include <assert.h>
#include <string.h>

#include "platform.h"
#include "logging.h"
#include "ringbuffer.h"

RingBuffer::RingBuffer(int startingSampleRate, int size)
    : capacity(size), readIndex(0), writeIndex(0), sampleRate(startingSampleRate)
{
    buffer = new float[size];

    // TODO: Note that this memset masks an error whereby we read before writing when we
    //       start listening to input (on the listenBuffer).
    //       We can reliably reproduce the issue by simply setting everything to some very large
    //       value (10e10) and looking at the output wave.
    memset(buffer, 0, sizeof(float)*size);

    lock = Platform::CreateMutex();
}

RingBuffer::~RingBuffer()
{
    Platform::DestroyMutex(lock);
    delete[] buffer;
}

void RingBuffer::write(int valCount, float* vals)
{
    for(int i=0; i<valCount; i++)
    {
        write(vals[i]);
    }
}

void RingBuffer::write(float value)
{
    Platform::LockMutex(lock);

    if(freeInternal() == 0)
    {
        readIndex++;
        if(readIndex >= capacity)
        {
            readIndex -= capacity;
        }
    }

    buffer[writeIndex] = value;

    writeIndex++;
    if(writeIndex >= capacity)
    {
        writeIndex -= capacity;
    }
    Platform::UnlockMutex(lock);
}

int RingBuffer::read(int valCount, float* values)
{
    int valsRead = 0;
    for(int i=0; i<valCount; i++)
    {
        float x = 0.0f;
        int readCount = read(&x);
        if(readCount == 0)
            break;

        valsRead++;
        values[i] = x;
    }
    return valsRead;
}

int RingBuffer::read(float* value)
{
    Platform::LockMutex(lock);
    if(writeIndex == readIndex)
    {
        return 0;
    }

    *value = buffer[readIndex];
    readIndex++;
    if(readIndex >= capacity)
    {
        readIndex -= capacity;
    }

    Platform::UnlockMutex(lock);

    return 1;
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
