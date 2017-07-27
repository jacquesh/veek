#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "ringbuffer.h"

TEST_CASE("Single value read/write")
{
    float xIn = 3.14f;
    RingBuffer* buffer = new RingBuffer(2);
    buffer->write(1, &xIn);

    float xOut;
    buffer->read(1, &xOut);
    REQUIRE(xOut == xIn);
}
