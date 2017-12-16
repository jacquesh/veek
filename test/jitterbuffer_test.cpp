#include <stdint.h>

#include "catch.hpp"
#include "jitterbuffer.h"
#include "logging.h"

// TODO: Note that these tests assume a hard-coded jitterbuffer size of 3

TEST_CASE("Get returns nothing until the buffer has been populated")
{
    JitterBuffer jb;
    uint8_t inVal = 3;
    uint8_t* outVal;

    REQUIRE(jb.Get(outVal) == 0);

    jb.Add(1, 1, &inVal);
    REQUIRE(jb.Get(outVal) == 0);

    jb.Add(2, 1, &inVal);
    REQUIRE(jb.Get(outVal) == 0);
}

TEST_CASE("Get returns data after the buffer has been populated")
{
    JitterBuffer jb;
    uint8_t inVal = 3;
    uint8_t* outVal;

    jb.Add(1, 1, &inVal);
    jb.Add(2, 1, &inVal);
    jb.Add(3, 1, &inVal);

    REQUIRE(jb.Get(outVal) == 1);
}

TEST_CASE("Get returns nothing when the data is all consumed after being populated")
{
    JitterBuffer jb;
    uint8_t inVal = 3;
    uint8_t* outVal;

    jb.Add(1, 1, &inVal);
    jb.Add(2, 1, &inVal);
    jb.Add(3, 1, &inVal);

    CHECK(jb.Get(outVal) == 1);
    CHECK(jb.Get(outVal) == 1);
    CHECK(jb.Get(outVal) == 1);
    REQUIRE(jb.Get(outVal) == 0);
}

TEST_CASE("Get returns data in the correct order after it was received in the correct order")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;

    inVal = 3;
    jb.Add(1, 1, &inVal);

    inVal = 4;
    jb.Add(2, 1, &inVal);

    inVal = 5;
    jb.Add(3, 1, &inVal);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 5);
}

TEST_CASE("Get returns data in the correct order after it was received with two items swapped around")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;

    inVal = 3;
    jb.Add(1, 1, &inVal);

    inVal = 5;
    jb.Add(3, 1, &inVal);

    inVal = 4;
    jb.Add(2, 1, &inVal);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 5);
}

TEST_CASE("Get returns data in the correct order after it was received in reverse order")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;

    inVal = 5;
    jb.Add(3, 1, &inVal);

    inVal = 4;
    jb.Add(2, 1, &inVal);

    inVal = 3;
    jb.Add(1, 1, &inVal);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 5);
}

TEST_CASE("Get returns empty data when a packet is dropped")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;

    inVal = 3;
    jb.Add(1, 1, &inVal);

    inVal = 5;
    jb.Add(3, 1, &inVal);

    inVal = 6;
    jb.Add(4, 1, &inVal);

    outCount = jb.Get(outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 3);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 0);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 5);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 6);
}

TEST_CASE("Get returns the expected packet after an extra packet is received after refill completes")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;

    inVal = 3;
    jb.Add(1, 1, &inVal);

    inVal = 4;
    jb.Add(2, 1, &inVal);

    inVal = 5;
    jb.Add(3, 1, &inVal);

    inVal = 6;
    jb.Add(4, 1, &inVal);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);
}

TEST_CASE("Packet index overflow is handled smoothly")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;
    uint16_t currentIndex = (1 << 16) - 2;

    inVal = 2;
    jb.Add(currentIndex++, 1, &inVal);
    inVal = 3;
    jb.Add(currentIndex++, 1, &inVal);
    inVal = 4;
    jb.Add(currentIndex++, 1, &inVal);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 2);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);
}
