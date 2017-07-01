#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "user.h"
#include "ringbuffer.h"

namespace Audio
{
    struct NetworkAudioPacket
    {
        UserIdentifier srcUser;
        uint8 index;
        uint16 encodedDataLength;
        uint8 encodedData[2400]; // TODO: Sizing (currently =2400=micBufferLen from main.cpp)

        template<typename Packet> bool serialize(Packet& packet);
    };

    const int32 NETWORK_SAMPLE_RATE = 48000;

    bool Setup();
    void Update();
    void Shutdown();

    int InputDeviceCount();
    const char** InputDeviceNames();
    bool SetAudioInputDevice(int newInputDevice);

    int OutputDeviceCount();
    const char** OutputDeviceNames();
    bool SetAudioOutputDevice(int newOutputDevice);

    void ListenToInput(bool listen);
    void PlayTestSound();

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

} // Audio

#endif
