#include "catch.hpp"

#include "ringbuffer.h"

TEST_CASE("A single value gets read after being written")
{
    float xIn = 3.14f;
    RingBuffer buffer = RingBuffer(1, 2);
    buffer.write(xIn);

    float xOut;
    buffer.read(&xOut);

    REQUIRE(xOut == xIn);
}

TEST_CASE("Multiple values are correctly read after being written")
{
    float xIn[3] = {3.14f, 2.71f, -1.0f};
    RingBuffer buffer = RingBuffer(1, 5);

    buffer.write(xIn[0]);
    buffer.write(xIn[1]);
    buffer.write(xIn[2]);

    float xOut[3] = {};
    buffer.read(&xOut[0]);
    buffer.read(&xOut[1]);
    buffer.read(&xOut[2]);

    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == xIn[2]);
}

TEST_CASE("A buffer with capacity n can store n-1 items")
{
    float xIn[3] = {3.14f, 2.71f, -1.0f};
    RingBuffer buffer = RingBuffer(1, 4);

    buffer.write(xIn[0]);
    buffer.write(xIn[1]);
    buffer.write(xIn[2]);

    float xOut[3] = {};
    buffer.read(&xOut[0]);
    buffer.read(&xOut[1]);
    buffer.read(&xOut[2]);

    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == xIn[2]);
}

TEST_CASE("The latest values are read when we write more than the full capacity")
{
    float xIn[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    RingBuffer buffer = RingBuffer(1, 3);

    buffer.write(xIn[0]);
    buffer.write(xIn[1]);
    buffer.write(xIn[2]);
    buffer.write(xIn[3]);
    buffer.write(xIn[4]);

    float xOut[2] = {};
    buffer.read(&xOut[0]);
    buffer.read(&xOut[1]);
    buffer.read(&xOut[2]);

    REQUIRE(xOut[0] == xIn[3]);
    REQUIRE(xOut[1] == xIn[4]);
}

TEST_CASE("Return fewer than the requested number of values when there is not enough data available (with a non-wrapping read)")
{
    float xIn[2] = {1.0f, 2.0f};
    RingBuffer buffer = RingBuffer(1, 5);

    buffer.write(xIn[0]);
    buffer.write(xIn[1]);

    float xOut[3] = {};
    int valuesRead = 0;
    valuesRead += buffer.read(&xOut[0]);
    valuesRead += buffer.read(&xOut[1]);
    valuesRead += buffer.read(&xOut[2]);

    REQUIRE(valuesRead == 2);
    REQUIRE(xOut[0] == xIn[0]);
    REQUIRE(xOut[1] == xIn[1]);
    REQUIRE(xOut[2] == 0.0f);
}

TEST_CASE("Return no values when there isn't any data available")
{
    float xIn[3] = {1.0f, 2.0f, 3.0f};
    RingBuffer buffer = RingBuffer(1, 4);

    buffer.write(xIn[0]);
    buffer.write(xIn[1]);
    buffer.write(xIn[2]);

    float xOut;
    int valuesRead;

    valuesRead = buffer.read(&xOut);
    REQUIRE(valuesRead == 1);
    REQUIRE(xOut == 1.0f);

    valuesRead = buffer.read(&xOut);
    REQUIRE(valuesRead == 1);
    REQUIRE(xOut == 2.0f);

    valuesRead = buffer.read(&xOut);
    REQUIRE(valuesRead == 1);
    REQUIRE(xOut == 3.0f);

    valuesRead = buffer.read(&xOut);
    REQUIRE(valuesRead == 0);
    valuesRead = buffer.read(&xOut);
    REQUIRE(valuesRead == 0);
    REQUIRE(xOut == 3.0f);
}

TEST_CASE("Read returns 0 values on a new buffer")
{
    RingBuffer buffer = RingBuffer(1, 5);

    float xOut;
    int valuesRead = buffer.read(&xOut);

    REQUIRE(valuesRead == 0);
}

TEST_CASE("Read returns 0 values on an empty buffer that has been written to and read from")
{
    float xIn[2] = {1.0f, 2.0f};
    RingBuffer buffer = RingBuffer(1, 5);
    buffer.write(xIn[0]);
    buffer.write(xIn[1]);

    float xOut;
    buffer.read(&xOut);
    buffer.read(&xOut);

    int valuesRead = buffer.read(&xOut);
    REQUIRE(valuesRead == 0);
}

TEST_CASE("Read advances the read pointer correctly")
{
    RingBuffer buffer = RingBuffer(1, 5);

    float xIn[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    buffer.write(xIn[0]);
    buffer.write(xIn[1]);
    buffer.write(xIn[2]);
    buffer.write(xIn[3]);

    float xOut[4];
    int valuesRead = 0;
    valuesRead += buffer.read(&xOut[0]);
    valuesRead += buffer.read(&xOut[1]);
    valuesRead += buffer.read(&xOut[2]);
    valuesRead += buffer.read(&xOut[3]);
    REQUIRE(valuesRead == 4);
    REQUIRE(xOut[0] == 1.0f);
    REQUIRE(xOut[1] == 2.0f);
    REQUIRE(xOut[2] == 3.0f);
    REQUIRE(xOut[3] == 4.0f);
}
