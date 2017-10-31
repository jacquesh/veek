#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

#include "soundio/soundio.h"

#include "user.h"

#ifndef _USER_CLIENT_H
// TODO: This is a really sucky solution. We actually need a user_client_defs.h or something.
//       This only works if you import audio.h before user_client.h
struct ClientUserData;
#endif

namespace Audio
{
    // NOTE: This should a sample rate that is supported by opus (e.g 48k, 24k)
    const int32 NETWORK_SAMPLE_RATE = 48000;

    struct AudioBuffer
    {
        float* Data;
        int Capacity;
        int Length;

        int SampleRate;

        AudioBuffer() = default;
        explicit AudioBuffer(int initialCapacity);
        ~AudioBuffer();
    };

    enum class MicActivationMode
    {
        Always = 0,
        PushToTalk,
        Automatic
    };

    struct NetworkAudioPacket
    {
        UserIdentifier srcUser;
        uint8 index;
        uint16 encodedDataLength;
        uint8 encodedData[2400]; // TODO: Sizing (currently =2400=micBufferLen from main.cpp)

        template<typename Packet> bool serialize(Packet& packet);
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

    void GenerateToneInput(bool generateTone);
    void ListenToInput(bool listen);
    void PlayTestSound();

    bool IsMicrophoneActive();
    float GetInputVolume();

    /**
     * \return The new state of the microphone, which equals enabled if the function succeeded,
     * and equals the previous state if the function failed
     */
    bool enableMicrophone(bool enabled);

    /**
     * \return The new state of the speakers, which equals enabled if the function succeeded,
     * and equals the previous state if the function failed
     */
    bool enableSpeakers(bool enabled);

    void AddAudioUser(UserIdentifier userId);
    void RemoveAudioUser(UserIdentifier userId);

    void ProcessIncomingPacket(NetworkAudioPacket& packet);

    void SendAudioToUser(ClientUserData* user, AudioBuffer& sourceBuffer);

    // TODO: Maybe return true/false to indicate if the buffer was large enough?
    // Returns the number of samples written to the buffer (samples != indices, see TODO/NOTE)
    void readAudioInputBuffer(AudioBuffer& buffer);

    // Returns the root-mean-square amplitude of the samples in buffer.
    float ComputeRMS(AudioBuffer& buffer);

    void writeAudioToFile(int length, uint8_t* data);

} // Audio

#endif
