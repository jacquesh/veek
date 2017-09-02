#ifndef _AUDIO_RESAMPLE_H
#define _AUDIO_RESAMPLE_H

#include "audio.h"
#include "ringbuffer.h"

#define RESAMPLE_CONTEXT_MAX_OUTPUT_SAMPLES 8

struct ResampleStreamContext
{
    int InputSampleRate;
    int OutputSampleRate;

    float InterSampleTime;

    bool HasPreviousSample;
    float PreviousInputSample;

    float PreviousOutputSamples[RESAMPLE_CONTEXT_MAX_OUTPUT_SAMPLES];
    int PreviousOutputSampleCount;
    int PreviousOutputSampleIndex;
};

int resampleStream(ResampleStreamContext& ctx,
                   float inputSample, float* outputSamples, int maxOutputSamples);

bool resampleStreamRequiresInput(ResampleStreamContext& ctx);
void resampleStreamInput(ResampleStreamContext& ctx, float inputSample);
float resampleStreamOutput(ResampleStreamContext& ctx);

float resampleStreamFrom(ResampleStreamContext& ctx, RingBuffer& buffer);

#endif // _AUDIO_RESAMPLE_H
