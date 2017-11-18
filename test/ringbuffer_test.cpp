#include "catch.hpp"

#include "ringbuffer.h"

TEST_CASE("A single value gets read after being written")
{
    float xIn = 3.14f;
    RingBuffer buffer = RingBuffer(1, 2);
    buffer.write(1, &xIn);

    float xOut;
    buffer.read(1, &xOut);

    REQUIRE(xOut == xIn);
}

TEST_CASE("Multiple values are correctly read after being written")
{
    float xIn[3] = {3.14f, 2.71f, -1.0f};
    RingBuffer buffer = RingBuffer(1, 5);

    buffer.write(3, &xIn[0]);

    float xOut[3];
    buffer.read(3, &xOut[0]);

    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == xIn[2]);
}

TEST_CASE("A buffer with capacity n can store n-1 items")
{
    float xIn[3] = {3.14f, 2.71f, -1.0f};
    RingBuffer buffer = RingBuffer(1, 4);

    buffer.write(3, &xIn[0]);

    float xOut[3];
    buffer.read(3, &xOut[0]);

    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == xIn[2]);
}

TEST_CASE("The latest values are read when we write more than the full capacity")
{
    float xIn[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    RingBuffer buffer = RingBuffer(1, 3);

    buffer.write(2, &xIn[0]);
    buffer.write(2, &xIn[2]);
    buffer.write(1, &xIn[4]);

    float xOut[2];
    buffer.read(2, &xOut[0]);

    REQUIRE(xOut[0] == xIn[3]);
    REQUIRE(xOut[1] == xIn[4]);
}

TEST_CASE("Return fewer than the requested number of values when there is not enough data available (with a non-wrapping read)")
{
    float xIn[2] = {1.0f, 2.0f};
    RingBuffer buffer = RingBuffer(1, 5);

    buffer.write(2, &xIn[0]);

    float xOut[3] = {};
    int valuesRead = buffer.read(3, &xOut[0]);

    REQUIRE(valuesRead == 2);
    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == 0.0f);
}

TEST_CASE("Return fewer than the requested number of values when there is not enough data available (with a wrapping read)")
{
    float xIn[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    RingBuffer buffer = RingBuffer(1, 5);

    float xOut[4] = {};
    buffer.write(4, &xIn[0]);
    buffer.read(3, &xOut[0]);
    buffer.write(2, &xIn[0]);

    int valuesRead = buffer.read(4, &xOut[0]);

    REQUIRE(valuesRead == 3);
    REQUIRE(xOut[0] == xIn[3]);
    REQUIRE(xOut[1] == xIn[0]);
    REQUIRE(xOut[2] == xIn[1]);
    REQUIRE(xOut[3] == 0.0f);
}

TEST_CASE("Read returns 0 values on a new buffer")
{
    RingBuffer buffer = RingBuffer(1, 5);

    float xOut[2] = {};
    int valuesRead = buffer.read(2, &xOut[0]);

    REQUIRE(valuesRead == 0);
}

TEST_CASE("Read returns 0 values on an empty buffer that has been written to and read from")
{
    float xIn[2] = {1.0f, 2.0f};
    RingBuffer buffer = RingBuffer(1, 5);
    buffer.write(2, &xIn[0]);

    float xOut[2] = {};
    buffer.read(2, &xOut[0]);

    int valuesRead = buffer.read(2, &xOut[0]);
    REQUIRE(valuesRead == 0);
}

TEST_CASE("Read advances the read pointer correctly")
{
    RingBuffer buffer = RingBuffer(1, 5);

    float xIn[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    buffer.write(4, &xIn[0]);

    float xOut[2];
    int valuesRead = buffer.read(2, &xOut[0]);
    REQUIRE(valuesRead == 2);
    REQUIRE(xOut[0] == 1.0f);
    REQUIRE(xOut[1] == 2.0f);

    valuesRead = buffer.read(2, &xOut[0]);
    REQUIRE(valuesRead == 2);
    REQUIRE(xOut[0] == 3.0f);
    REQUIRE(xOut[1] == 4.0f);
}
