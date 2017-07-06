#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "ringbuffer.h"
#include "user.h"

#ifndef _USER_CLIENT_H
// TODO: This is a really sucky solution. We actually need a user_client_defs.h or something.
//       This only works if you import audio.h before user_client.h
struct ClientUserData;
#endif

namespace Audio
{
    const int32 NETWORK_SAMPLE_RATE = 48000;

    struct NetworkAudioPacket
    {
        UserIdentifier srcUser;
        uint8 index;
        uint16 encodedDataLength;
        uint8 encodedData[2400]; // TODO: Sizing (currently =2400=micBufferLen from main.cpp)

        template<typename Packet> bool serialize(Packet& packet);
    };

    struct UserAudioData
    {
        int32 sampleRate;
        OpusDecoder* decoder;
        RingBuffer* buffer;
    };

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

    void SendAudioToUser(ClientUserData* user, int sourceLength, float* sourceBuffer);

    // TODO: It might be better to take the encoder here, as with decodePacket
    int encodePacket(int sourceLength, float* sourceBuffer, int sourceSampleRate, int targetLength, uint8_t* targetBuffer);
    int decodePacket(OpusDecoder* decoder, int sourceLength, uint8_t* sourceBuffer, int targetLength, float* targetBuffer, int targetSampleRate);

    // Returns the number of samples written to the buffer (samples != indices, see TODO/NOTE)
    int readAudioInputBuffer(int bufferLength, float* buffer);

    void writeAudioToFile(int length, uint8_t* data);

} // Audio

#endif
