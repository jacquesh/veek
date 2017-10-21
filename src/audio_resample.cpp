#include <assert.h>

#include "audio.h"
#include "audio_resample.h"
#include "logging.h"
#include "math_utils.h"

// TODO: Improve resample speed/quality.
//       E.g Opus FAQ (https://wiki.xiph.org/OpusFAQ) lists one from opus-tools:
//       https://github.com/xiph/opus-tools/blob/master/src/resample.c
//       https://ccrma.stanford.edu/~jos/resample/

static int resampleStream(ResampleStreamContext& ctx,
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

static bool resampleStreamRequiresInput(ResampleStreamContext& ctx)
{
    return ctx.PreviousOutputSampleIndex >= ctx.PreviousOutputSampleCount;
}

static void resampleStreamInput(ResampleStreamContext& ctx, float inputSample)
{
    int outCount = resampleStream(ctx, inputSample, &ctx.PreviousOutputSamples[0],
                                  RESAMPLE_CONTEXT_MAX_OUTPUT_SAMPLES);
    ctx.PreviousOutputSampleIndex = 0;
    ctx.PreviousOutputSampleCount = outCount;
}

static float resampleStreamOutput(ResampleStreamContext& ctx)
{
    assert(ctx.PreviousOutputSampleIndex < ctx.PreviousOutputSampleCount);

    float result = ctx.PreviousOutputSamples[ctx.PreviousOutputSampleIndex];
    ctx.PreviousOutputSampleIndex++;
    return result;
}

void resampleBuffer2Buffer(ResampleStreamContext& ctx,
                           const Audio::AudioBuffer& input,
                           Audio::AudioBuffer& output)
{
    // TODO: Check that we have enough space for the output, and that we don't overflow.
    ctx.InputSampleRate = input.SampleRate;
    ctx.OutputSampleRate = output.SampleRate;

    output.Length = 0;
    for(int i=0; i<input.Length; i++)
    {
        resampleStreamInput(ctx, input.Data[i]);
        while(!resampleStreamRequiresInput(ctx))
        {
            float sample = resampleStreamOutput(ctx);
            output.Data[output.Length++] = sample;
        }
    }
}

void resampleBuffer2Ring(ResampleStreamContext& ctx,
                         const Audio::AudioBuffer& input,
                         RingBuffer& output)
{
    ctx.InputSampleRate = input.SampleRate;

    for(int i=0; i<input.Length; i++)
    {
        resampleStreamInput(ctx, input.Data[i]);
        while(!resampleStreamRequiresInput(ctx))
        {
            float sample = resampleStreamOutput(ctx);
            output.write(1, &sample);
        }
    }
}
