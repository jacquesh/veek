#ifndef _AUDIO_RESAMPLE_H
#define _AUDIO_RESAMPLE_H

#include "audio.h"

struct ResampleStreamContext
{
    int InputSampleRate;
    int OutputSampleRate;

    float InterSampleTime;

    float PreviousSample;
};

int resampleStream(ResampleStreamContext& ctx,
                   float inputSample, float* outputSamples, int maxOutputSamples);

void resampleBuffer(Audio::AudioBuffer& inputBuffer, Audio::AudioBuffer& outputBuffer);

#endif // _AUDIO_RESAMPLE_H
