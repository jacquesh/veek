#include <assert.h>

#include "audio_resample.h"
#include "logging.h"
#include "math_utils.h"

// TODO: Improve resample speed/quality.
//       E.g Opus FAQ (https://wiki.xiph.org/OpusFAQ) lists one from opus-tools:
//       https://github.com/xiph/opus-tools/blob/master/src/resample.c
//       https://ccrma.stanford.edu/~jos/resample/

bool resampleStreamRequiresInput(ResampleStreamContext& ctx)
{
    return ctx.PreviousOutputSampleIndex >= ctx.PreviousOutputSampleCount;
}

void resampleStreamInput(ResampleStreamContext& ctx, float inputSample)
{
    int outCount = resampleStream(ctx, inputSample, &ctx.PreviousOutputSamples[0],
                                  RESAMPLE_CONTEXT_MAX_OUTPUT_SAMPLES);
    ctx.PreviousOutputSampleIndex = 0;
    ctx.PreviousOutputSampleCount = outCount;
}

float resampleStreamOutput(ResampleStreamContext& ctx)
{
    assert(ctx.PreviousOutputSampleIndex < ctx.PreviousOutputSampleCount);

    float result = ctx.PreviousOutputSamples[ctx.PreviousOutputSampleIndex];
    ctx.PreviousOutputSampleIndex++;
    return result;
}

float resampleStreamFrom(ResampleStreamContext& ctx, RingBuffer& buffer)
{
    if(resampleStreamRequiresInput(ctx))
    {
        float val;
        buffer.read(1, &val);
        resampleStreamInput(ctx, val);
    }

    return resampleStreamOutput(ctx);
}

int resampleStream(ResampleStreamContext& ctx,
                   float inputSample, float* outputSamples, int maxOutputSamples)
{
    // TODO: Apparently its a good idea to lowpass filter the output here
    //       We can achieve that with a simple averaging
    float inTimePerSample = 1.0f/(float)ctx.InputSampleRate;
    float outTimePerSample = 1.0f/(float)ctx.OutputSampleRate;

    if(!ctx.HasPreviousSample)
    {
        ctx.PreviousInputSample = inputSample;
        ctx.HasPreviousSample = true;
        ctx.InterSampleTime += outTimePerSample;
        outputSamples[0] = inputSample;
        return 1;
    }

    int outputSampleIndex = 0;
    while(ctx.InterSampleTime <= inTimePerSample)
    {
        if(outputSampleIndex >= maxOutputSamples)
        {
            logWarn("Unable to fit all resample output samples into the given buffer of size %d\n", maxOutputSamples);
            break;
        }

        float t = ctx.InterSampleTime/inTimePerSample;
        float newSample = lerp(ctx.PreviousInputSample, inputSample, t);
        outputSamples[outputSampleIndex++] = newSample;

        ctx.InterSampleTime += outTimePerSample;
    }

    ctx.InterSampleTime -= inTimePerSample;
    ctx.PreviousInputSample = inputSample;
    return outputSampleIndex;
}
