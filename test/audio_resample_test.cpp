#include "catch.hpp"

#include "audio_resample.h"

TEST_CASE("Upsample first input matches output")
{
    ResampleStreamContext ctx = {};
    ctx.InputSampleRate = 10;
    ctx.OutputSampleRate = 15;

    float inputVal = 3.14f;
    float outSamples[8];
    int outSampleCount = resampleStream(ctx, inputVal, outSamples, 8);

    REQUIRE(outSampleCount == 1);
    REQUIRE(outSamples[0] == inputVal);
}

TEST_CASE("Upsample second input gives some output")
{
    ResampleStreamContext ctx = {};
    ctx.InputSampleRate = 10;
    ctx.OutputSampleRate = 15;

    float inputVals[2] = {1.0f, 2.0f};

    float outSamples[8];
    int outSampleCount = resampleStream(ctx, inputVals[0], outSamples, 8);
    outSampleCount = resampleStream(ctx, inputVals[1], outSamples, 8);

    REQUIRE(outSampleCount == 1);
}

TEST_CASE("Downsample first input matches output")
{
    ResampleStreamContext ctx = {};
    ctx.InputSampleRate = 15;
    ctx.OutputSampleRate = 10;

    float inputVal = 3.14f;
    float outSamples[8];
    int outSampleCount = resampleStream(ctx, inputVal, outSamples, 8);

    REQUIRE(outSampleCount == 1);
    REQUIRE(outSamples[0] == inputVal);
}

TEST_CASE("Downsample second input gives no output")
{
    ResampleStreamContext ctx = {};
    ctx.InputSampleRate = 15;
    ctx.OutputSampleRate = 10;

    float inputVals[2] = {1.0f, 2.0f};

    float outSamples[8];
    int outSampleCount = resampleStream(ctx, inputVals[0], outSamples, 8);
    outSampleCount = resampleStream(ctx, inputVals[1], outSamples, 8);

    REQUIRE(outSampleCount == 0);
}

TEST_CASE("Resampling to the current sample rate returns output 1-for-1")
{
    ResampleStreamContext ctx = {};
    ctx.InputSampleRate = 48000;
    ctx.OutputSampleRate = 48000;

    float inputVals[3] = {1.0f, 2.0f, 3.0f};
    int outSampleCount;
    float outSamples[2];

    outSampleCount = resampleStream(ctx, inputVals[0], outSamples, 2);
    REQUIRE(outSampleCount == 1);

    outSampleCount = resampleStream(ctx, inputVals[1], outSamples, 2);
    REQUIRE(outSampleCount == 1);

    outSampleCount = resampleStream(ctx, inputVals[2], outSamples, 2);
    REQUIRE(outSampleCount == 1);
}
