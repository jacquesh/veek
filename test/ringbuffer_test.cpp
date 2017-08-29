#include "catch.hpp"

#include "ringbuffer.h"

TEST_CASE("A single value gets read after being written")
{
    float xIn = 3.14f;
    RingBuffer* buffer = new RingBuffer(2);
    buffer->write(1, &xIn);

    float xOut;
    buffer->read(1, &xOut);
    delete buffer;

    REQUIRE(xOut == xIn);
}

TEST_CASE("Multiple values are correctly read after being written")
{
    float xIn[3] = {3.14f, 2.71f, -1.0f};
    RingBuffer* buffer = new RingBuffer(5);

    buffer->write(3, &xIn[0]);

    float xOut[3];
    buffer->read(3, &xOut[0]);
    delete buffer;

    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == xIn[2]);
}

#if 0
TEST_CASE("The buffer can store as many items as its full capacity")
{
    float xIn[3] = {3.14f, 2.71f, -1.0f};
    RingBuffer* buffer = new RingBuffer(3);

    buffer->write(3, &xIn[0]);

    float xOut[3];
    buffer->read(3, &xOut[0]);
    delete buffer;

    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == xIn[2]);
}
#endif

TEST_CASE("The latest values are read when we write more than the full capacity")
{
    float xIn[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    RingBuffer* buffer = new RingBuffer(3);

    buffer->write(2, &xIn[0]);
    buffer->write(2, &xIn[2]);
    buffer->write(1, &xIn[4]);

    float xOut[2];
    buffer->read(2, &xOut[0]);
    delete buffer;

    REQUIRE(xOut[0] == xIn[3]);
    REQUIRE(xOut[1] == xIn[4]);
}
