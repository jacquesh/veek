#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

#include "soundio/soundio.h"

struct AudioData
{
    int inputDeviceCount;
    SoundIoDevice** inputDeviceList;
    char** inputDeviceNames;
    int defaultInputDevice;
    int currentInputDevice;
    bool inputEnabled;

    int outputDeviceCount;
    SoundIoDevice** outputDeviceList;
    char** outputDeviceNames;
    int defaultOutputDevice;
    int currentOutputDevice;
    int outputEnabled;
};

extern AudioData audioState;

bool initAudio();
void deinitAudio();

void setAudioInputDevice(int newInputDevice);
void setAudioOutputDevice(int newOutputDevice);

void enableMicrophone(bool enabled);

int encodePacket(int sourceLength, float* sourceBuffer, int targetLength, uint8_t* targetBuffer);
int decodePacket(int sourceLength, uint8_t* sourceBuffer, int targetLength, float* targetBuffer);

// Returns the number of samples written to the buffer (samples != indices, see TODO/NOTE)
int readAudioInputBuffer(int bufferLength, float* buffer);
int writeAudioOutputBuffer(int bufferLength, float* buffer);

void writeAudioToFile(int length, uint8_t* data);
#endif
