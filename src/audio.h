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

void readToAudioOutputBuffer(uint32_t timestamp, uint32_t length, uint8_t* data);
void writeFromAudioInputBuffer(uint32_t bufferSize, uint8_t* buffer);

#endif
