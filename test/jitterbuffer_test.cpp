#include <stdint.h>

#include "catch.hpp"
#include "jitterbuffer.h"

TEST_CASE("Get returns nothing until data has been added")
{
    JitterBuffer jb;
    uint8_t inVal = 3;
    uint8_t* outVal;

    REQUIRE(jb.Get(&outVal) == 0);

    jb.Add(1, 1, &inVal);
    jb.Add(2, 1, &inVal);
    REQUIRE(jb.Get(&outVal) == 1);
    REQUIRE(*outVal == 3);
}

TEST_CASE("Get returns data after some has been added")
{
    JitterBuffer jb;
    uint8_t inVal = 3;
    uint8_t* outVal;

    jb.Add(1, 1, &inVal);

    REQUIRE(jb.Get(&outVal) == 1);
}

TEST_CASE("Get returns nothing when the data is all consumed after being added")
{
    JitterBuffer jb;
    uint8_t inVal = 3;
    uint8_t* outVal;

    jb.Add(1, 1, &inVal);
    jb.Add(2, 1, &inVal);
    jb.Add(3, 1, &inVal);

    CHECK(jb.Get(&outVal) == 1);
    CHECK(jb.Get(&outVal) == 1);
    CHECK(jb.Get(&outVal) == 1);
    REQUIRE(jb.Get(&outVal) == 0);
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

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);

    outCount = jb.Get(&outVal);
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

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);

    outCount = jb.Get(&outVal);
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

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 5);
}

TEST_CASE("Get returns empty data where the packet would be when a packet is dropped")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;

    inVal = 3;
    jb.Add(1, 1, &inVal);

    inVal = 5;
    jb.Add(3, 1, &inVal);

    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 3);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 0);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 5);
}

TEST_CASE("Packet index overflow is handled smoothly")
{
    JitterBuffer jb;
    uint8_t inVal;
    uint16_t outCount;
    uint8_t* outVal;
    uint16_t startIndex = (1 << 16) - 2;
    uint16_t currentIndex = startIndex;
    // NOTE: This call will return with no data because there is no data, but it sets the correct
    //       expected packet index.
    outCount = jb.Get(startIndex-1, &outVal);

    inVal = 2;
    jb.Add(currentIndex++, 1, &inVal);
    inVal = 3;
    jb.Add(currentIndex++, 1, &inVal);
    inVal = 4;
    jb.Add(currentIndex++, 1, &inVal);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 2);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);
}

TEST_CASE("The new packet is dropped when the buffer is full and a new packet is added")
{
    JitterBuffer jb;
    uint8_t inVal;

    inVal = 1;
    jb.Add(1, 1, &inVal);
    inVal = 2;
    jb.Add(2, 1, &inVal);
    inVal = 3;
    jb.Add(3, 1, &inVal);
    inVal = 4;
    jb.Add(4, 1, &inVal);
    inVal = 5;
    jb.Add(5, 1, &inVal);
    inVal = 6;
    jb.Add(6, 1, &inVal);
    inVal = 7;
    jb.Add(7, 1, &inVal);

    uint8_t* outVal;
    uint16_t outCount;
    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 1);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 2);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 3);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 4);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 5);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 6);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 0);
}

TEST_CASE("When adding a packet more than once, the second packet is ignored")
{
    JitterBuffer jb;
    uint8_t inVal;
    inVal = 1;
    jb.Add(1, 1, &inVal);
    jb.Add(1, 1, &inVal);

    uint8_t* outVal;
    uint16_t outCount;
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    REQUIRE(*outVal == 1);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 0);
}

TEST_CASE("When the first packet to be added to an empty buffer is dropped, it causes a gap in the output")
{
    JitterBuffer jb;
    uint8_t inVal;
    inVal = 5;
    jb.Add(2, 1, &inVal);

    uint8_t* outVal;
    uint16_t outCount;
    outCount = jb.Get(&outVal);
    CHECK(outCount == 0);

    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    REQUIRE(*outVal == 5);
}

TEST_CASE("When a packet is added after it was requested (without an empty buffer), it is ignored")
{
    JitterBuffer jb;
    uint8_t inVal;
    inVal = 10;
    jb.Add(1, 1, &inVal);
    inVal = 12;
    jb.Add(3, 1, &inVal);

    uint8_t* outVal;
    uint16_t outCount;
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    REQUIRE(*outVal == 10);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 0);

    inVal = 11;
    jb.Add(2, 1, &inVal);

    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    REQUIRE(*outVal == 12);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 0);
}

TEST_CASE("When a packet is added after it was requested (with an empty buffer), it is ignored")
{
    JitterBuffer jb;
    uint8_t* outVal;
    uint16_t outCount;
    outCount = jb.Get(&outVal);
    CHECK(outCount == 0);

    uint8_t inVal;
    inVal = 11;
    jb.Add(1, 1, &inVal);
    inVal = 12;
    jb.Add(2, 1, &inVal);

    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 12);
}

static void AddSingleByteToJitterBuffer(JitterBuffer& jb, uint16_t valueIndex, uint8_t valueToAdd)
{
    jb.Add(valueIndex, 1, &valueToAdd);
}

TEST_CASE("When a client stops sending for some time, only the packets that arrive after they were needed get dropped")
{
    JitterBuffer jb;
    uint8_t* outVal;
    int outCount;

    // Setup
    AddSingleByteToJitterBuffer(jb, 1, 11);
    AddSingleByteToJitterBuffer(jb, 2, 12);
    AddSingleByteToJitterBuffer(jb, 3, 13);

    // Normal functioning (works)
    AddSingleByteToJitterBuffer(jb, 4, 14);
    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 11);

    AddSingleByteToJitterBuffer(jb, 5, 15);
    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 12);

    AddSingleByteToJitterBuffer(jb, 6, 16);
    outCount = jb.Get(&outVal);
    REQUIRE(outCount == 1);
    REQUIRE(*outVal == 13);

    // Normal functioning (without packets coming in)
    // Skip insert of packet 7
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 14);

    // Skip insert of packet 8
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 15);

    // Skip insert of packet 9
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 16);

    // Skip insert of packet 10
    outCount = jb.Get(&outVal);
    CHECK(outCount == 0);
    // We expected to get packet 7

    // The remote user catches up and sends all the packets
    AddSingleByteToJitterBuffer(jb,  7, 17);
    AddSingleByteToJitterBuffer(jb,  8, 18);
    AddSingleByteToJitterBuffer(jb,  9, 19);
    AddSingleByteToJitterBuffer(jb, 10, 20);
    AddSingleByteToJitterBuffer(jb, 11, 21);
    AddSingleByteToJitterBuffer(jb, 12, 22);

    // Normal functioning (packets coming in)
    AddSingleByteToJitterBuffer(jb, 13, 23);
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 18);

    AddSingleByteToJitterBuffer(jb, 14, 24);
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 19);

    AddSingleByteToJitterBuffer(jb, 15, 25);
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 20);

    AddSingleByteToJitterBuffer(jb, 16, 26);
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 21);

    AddSingleByteToJitterBuffer(jb, 17, 27);
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 22);

    AddSingleByteToJitterBuffer(jb, 18, 28);
    outCount = jb.Get(&outVal);
    CHECK(outCount == 1);
    CHECK(*outVal == 23);
}
