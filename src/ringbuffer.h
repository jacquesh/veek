#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

/*
 * NOTE: This ring buffer implementation is not thread-safe, you should do your own mutual
 *       exclusion if you're using this from multiple threads
 */
class RingBuffer
{
public:
    RingBuffer(int size);
    ~RingBuffer();

    // Write a single value into the buffer
    void write(float val);

    // Write an array of values into the buffer
    void write(int valCount, float* vals);


    // Read a single value out of the buffer
    float read();

    // Read an array of values out of the buffer, writing them into the given vals array
    void read(int valCount, float* vals);


    void advanceWritePointer(int increment);
    void advanceReadPointer(int increment);

    // Returns the number of items that are available for reading in the buffer
    int count();

    // Returns the number of items that can be written without passing the read pointer
    int free();

private:
    int capacity;
    int _count;
    float* buffer;

    int readIndex;
    int writeIndex;
};

#endif
