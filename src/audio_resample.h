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

void resampleBuffer2Buffer(ResampleStreamContext& ctx,
                           const Audio::AudioBuffer& input,
                           Audio::AudioBuffer& output);
void resampleBuffer2Ring(ResampleStreamContext& ctx,
                         const Audio::AudioBuffer& input,
                         RingBuffer& output);

#endif // _AUDIO_RESAMPLE_H
