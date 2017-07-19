#include <assert.h>

#include "audio_resample.h"
#include "logging.h"
#include "math_utils.h"


// TODO: Improve resample speed/quality.
//       E.g Opus FAQ (https://wiki.xiph.org/OpusFAQ) lists one from opus-tools:
//       https://github.com/xiph/opus-tools/blob/master/src/resample.c

int resampleStream(ResampleStreamContext& ctx,
                   float inputSample, float* outputSamples, int maxOutputSamples)
{
    float inTimePerSample = 1.0f/(float)ctx.InputSampleRate;
    float outTimePerSample = 1.0f/(float)ctx.OutputSampleRate;

    int outputSampleIndex = 0;
    while(ctx.InterSampleTime < inTimePerSample)
    {
        if(outputSampleIndex >= maxOutputSamples)
        {
            logWarn("Unable to fit all resample output samples into the given buffer of size %d\n", maxOutputSamples);
            break;
        }

        float t = ctx.InterSampleTime/inTimePerSample;
        outputSamples[outputSampleIndex++] = lerp(ctx.PreviousSample, inputSample, t);
        ctx.InterSampleTime += outTimePerSample;
    }

    ctx.InterSampleTime -= inTimePerSample;
    ctx.PreviousSample = inputSample;
    return outputSampleIndex;
}

void resampleBuffer(Audio::AudioBuffer& inputBuffer, Audio::AudioBuffer& outputBuffer)
{
    assert(inputBuffer.Capacity > 0);
    assert(outputBuffer.Capacity > 0);
    outputBuffer.Data[0] = inputBuffer.Data[0];

    float inTimePerSample = 1.0f/(float)inputBuffer.SampleRate;
    float outTimePerSample = 1.0f/(float)outputBuffer.SampleRate;

    // TODO: Apparently its a good idea to lowpass filter the output here
    //       We can achieve that with a simple averaging
    int inIndex = 1;
    int outIndex = 1;
    float timeTillNextInSample = inTimePerSample;
    float timeTillNextOutSample = outTimePerSample;
    while((inIndex < inputBuffer.Capacity) && (outIndex < outputBuffer.Capacity))
    {
        float advanceTime = minf(timeTillNextInSample, timeTillNextOutSample);
        timeTillNextInSample -= advanceTime;
        timeTillNextOutSample -= advanceTime;

        if(timeTillNextInSample <= 0.0f)
        {
            timeTillNextInSample += inTimePerSample;
            inIndex++;
        }
        if(timeTillNextOutSample <= 0.0f)
        {
            timeTillNextOutSample += outTimePerSample;
            float previousInSample = inputBuffer.Data[inIndex-1];
            float nextInSample = inputBuffer.Data[inIndex];
            float lerpFactor = 1.0f - (timeTillNextInSample/inTimePerSample);
            outputBuffer.Data[outIndex] = lerp(previousInSample, nextInSample, lerpFactor);
            outIndex++;
        }
    }
    outputBuffer.Length = outIndex;

    // NOTE: If the inBuffer is shorter than the outBuffer, we just fill the rest of the
    //       outBuffer with silence
    while(outIndex < outputBuffer.Capacity)
    {
        outputBuffer.Data[outIndex++] = 0.0f;
    }
    //outputBuffer.Length = outputBuffer.Capacity;
}

