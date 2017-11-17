#include "catch.hpp"

#include "audio.h"
#include "audio_resample.h"

using namespace Audio;

// TODO: Dirty hack so we don't need to compile audio.cpp (which has its own dependencies)
Audio::AudioBuffer::AudioBuffer(int initialCapacity)
{
    Data = new float[initialCapacity];
    Capacity = initialCapacity;
    Length = 0;
}

Audio::AudioBuffer::~AudioBuffer()
{
    if(Data)
    {
        delete[] Data;
    }
}

TEST_CASE("Upsample first input matches output")
{
    ResampleStreamContext ctx = {};
    AudioBuffer inBuffer(1);
    AudioBuffer outBuffer(2);

    inBuffer.Data[0] = 3.14f;
    inBuffer.Length = 1;
    inBuffer.SampleRate = 10;
    outBuffer.SampleRate = 15;

    resampleBuffer2Buffer(ctx, inBuffer, outBuffer);

    REQUIRE(outBuffer.Length == 1);
    REQUIRE(outBuffer.Data[0] == inBuffer.Data[0]);
}

TEST_CASE("Upsample from two samples, gives the correct number of output samples")
{
    ResampleStreamContext ctx = {};
    AudioBuffer inBuffer(2);
    AudioBuffer outBuffer(5);

    inBuffer.Data[0] = 1.0f;
    inBuffer.Data[1] = 2.0f;
    inBuffer.Length = 2;
    inBuffer.SampleRate = 10;
    outBuffer.SampleRate = 15;

    resampleBuffer2Buffer(ctx, inBuffer, outBuffer);

    // NOTE: 2 is the correct number, we'll only be part-way to the 3rd output sample
    REQUIRE(outBuffer.Length == 2);
}

TEST_CASE("Downsample first input matches output")
{
    ResampleStreamContext ctx = {};
    AudioBuffer inBuffer(1);
    AudioBuffer outBuffer(2);

    inBuffer.Data[0] = 3.14f;
    inBuffer.Length = 1;
    inBuffer.SampleRate = 15;
    outBuffer.SampleRate = 10;

    resampleBuffer2Buffer(ctx, inBuffer, outBuffer);

    REQUIRE(outBuffer.Length == 1);
    REQUIRE(outBuffer.Data[0] == inBuffer.Data[0]);
}

TEST_CASE("Downsample from two samples, gives the correct number of output samples")
{
    ResampleStreamContext ctx = {};
    AudioBuffer inBuffer(2);
    AudioBuffer outBuffer(5);

    inBuffer.Data[0] = 1.0f;
    inBuffer.Data[1] = 2.0f;
    inBuffer.Length = 2;
    inBuffer.SampleRate = 15;
    outBuffer.SampleRate = 10;

    resampleBuffer2Buffer(ctx, inBuffer, outBuffer);

    REQUIRE(outBuffer.Length == 1);
}

TEST_CASE("Resampling to the current sample rate returns the input as-is")
{
    ResampleStreamContext ctx = {};
    AudioBuffer inBuffer(3);
    AudioBuffer outBuffer(3);

    inBuffer.Data[0] = 1.0f;
    inBuffer.Data[1] = 2.0f;
    inBuffer.Data[2] = 3.0f;
    inBuffer.Length = 3;
    inBuffer.SampleRate = 48000;
    outBuffer.SampleRate = 48000;

    resampleBuffer2Buffer(ctx, inBuffer, outBuffer);

    REQUIRE(outBuffer.Length == inBuffer.Length);
    REQUIRE(outBuffer.Data[0] == inBuffer.Data[0]);
    REQUIRE(outBuffer.Data[1] == inBuffer.Data[1]);
    REQUIRE(outBuffer.Data[2] == inBuffer.Data[2]);
}
