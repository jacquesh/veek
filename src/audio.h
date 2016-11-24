#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "user.h"
#include "ringbuffer.h"

struct NetworkAudioPacket
{
    UserIdentifier srcUser;
    uint8 index;
    uint16 encodedDataLength;
    uint8 encodedData[2400]; // TODO: Sizing (currently =2400=micBufferLen from main.cpp)

    template<typename Packet> bool serialize(Packet& packet);
};

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

struct AudioSource
{
    RingBuffer* buffer;
};

const int32 NETWORK_SAMPLE_RATE = 48000;

extern AudioData audioState; // TODO: We probably want to only have SOME of this be global
                             //       E.g We probably don't need/want global access to the 
                             //       enabled bools or the devices themselves etc

bool initAudio();
void updateAudio();
void initAudioUser(int userIndex); // TODO: Implementation. We need to do things when a user
void deinitAudioUser(int userIndex);//      connects so that we can create a decoder etc
void deinitAudio();

bool setAudioInputDevice(int newInputDevice);
bool setAudioOutputDevice(int newOutputDevice);

void listenToInput(bool listen);
void playTestSound();

/**
 * \return The new state of the microphone, which equals enabled if the function succeeded,
 * and equals the previous state if the function failed
 */
bool enableMicrophone(bool enabled);

// TODO: It might be better to take the encoder here, as with decodePacket
int encodePacket(int sourceLength, float* sourceBuffer, int sourceSampleRate, int targetLength, uint8_t* targetBuffer);
int decodePacket(OpusDecoder* decoder, int sourceLength, uint8_t* sourceBuffer, int targetLength, float* targetBuffer, int targetSampleRate);

// Returns the number of samples written to the buffer (samples != indices, see TODO/NOTE)
int readAudioInputBuffer(int bufferLength, float* buffer);

void writeAudioToFile(int length, uint8_t* data);
#endif
