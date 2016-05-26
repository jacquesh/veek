#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

#include "soundio/soundio.h"

struct AudioData
{
    int inputDeviceCount;
    SoundIoDevice** inputDeviceList;
    char** inputDeviceNames;
    int currentInputDevice;

    int outputDeviceCount;
    SoundIoDevice** outputDeviceList;
    char** outputDeviceNames;
    int currentOutputDevice;

    bool isListeningToInput;
};

extern AudioData audioState; // TODO: We probably want to only have SOME of this be global
                             //       E.g We probably don't need/want global access to the 
                             //       enabled bools or the devices themselves etc

bool initAudio();
void deinitAudio();

bool setAudioInputDevice(int newInputDevice);
bool setAudioOutputDevice(int newOutputDevice);

void listenToInput(bool listen);
void enableMicrophone(bool enabled);

int encodePacket(int sourceLength, float* sourceBuffer, int targetLength, uint8_t* targetBuffer);
int decodePacket(int sourceLength, uint8_t* sourceBuffer, int targetLength, float* targetBuffer);

// Returns the number of samples written to the buffer (samples != indices, see TODO/NOTE)
int readAudioInputBuffer(int bufferLength, float* buffer);
int writeAudioOutputBuffer(int bufferLength, float* buffer);

void writeAudioToFile(int length, uint8_t* data);
#endif
