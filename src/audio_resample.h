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

/// Resample the full contents of input into output, overwriting any of output's previous contents.
void resampleBuffer2Buffer(ResampleStreamContext& ctx,
                           const Audio::AudioBuffer& input,
                           Audio::AudioBuffer& output);

/// Resample the full contents of input into output.
void resampleBuffer2Ring(ResampleStreamContext& ctx,
                         const Audio::AudioBuffer& input,
                         RingBuffer& output);

/// Resample the full contents of input into output.
void resampleRing2Ring(ResampleStreamContext& ctx,
                       RingBuffer& input,
                       RingBuffer& output);
#endif // _AUDIO_RESAMPLE_H
