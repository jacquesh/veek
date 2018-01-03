#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unordered_map>

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "audio.h"
#include "audio_resample.h"
#include "common.h"
#include "jitterbuffer.h"
#include "logging.h"
#include "math_utils.h"
#include "network.h"
#include "network_client.h"
#include "platform.h"
#include "ringbuffer.h"
#include "unorderedlist.h"
#include "user.h"
#include "user_client.h"


static const int AUDIO_PACKET_DURATION_MS = 20;
static const int AUDIO_PACKET_FRAME_SIZE = (AUDIO_PACKET_DURATION_MS * Audio::NETWORK_SAMPLE_RATE)/1000;

// NOTE: The actual buffer size doesn't really matter as long as we can fit enough data
//       into the buffer at once. At 48KHz, (1 << 18) is ~5s of mono data, enough for us
//       to notice if we're somehow reliant on the size of the buffer.
static int RING_BUFFER_SIZE = 1 << 18;

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

    bool generateToneInput;
    bool isListeningToInput;
    ResampleStreamContext inputListenResampler;

    Audio::AudioBuffer encodingBuffer; // Used for resampling before encoding, when necessary
    Audio::AudioBuffer decodingBuffer; // Used for resampling after decoding, when necessary

    bool inputEnabled;

    float  currentBufferVolume;
    bool inputActive;
    Audio::MicActivationMode inputActivationMode;
};

struct UserAudioData
{
    int32 sampleRate;
    ResampleStreamContext receiveResampler;
    OpusDecoder* decoder;
    RingBuffer* buffer;
    JitterBuffer* jitter;

    uint64_t totalExpectedPackets;
    uint64_t lostPackets;
};

static AudioData audioState = {};

static ResampleStreamContext sendResampler;
static RingBuffer* presendBuffer;
static Audio::AudioBuffer micBuffer;

static SoundIo* soundio = 0;
static OpusEncoder* encoder = 0;

static SoundIoDevice* inDevice = 0;
static SoundIoInStream* inStream = 0;
static RingBuffer* inBuffer = 0;

static SoundIoDevice* outDevice = 0;
static SoundIoOutStream* outStream = 0;

static UnorderedList<RingBuffer*> sourceList(10);

static RingBuffer* listenBuffer;

static std::unordered_map<UserIdentifier, UserAudioData> audioUsers;

// NOTE: We assume all audio is mono.
//       Input devices are opened as mono and output devices have the same sample copied to all
//       channels, so they may as well be mono.

static void printDevice(SoundIoDevice* device, bool isDefault)
{
    const char* rawStr = device->is_raw ? "(RAW) " : "";
    const char* defaultStr = isDefault ? "(DEFAULT)" : "";
    logInfo("%s %s%s\n", device->name, rawStr, defaultStr);
    if(device->probe_error != SoundIoErrorNone)
    {
        logWarn("Probe Error: %s\n", soundio_strerror(device->probe_error));
        return;
    }
    logInfo("  Sample Rate: %d - %d (Currently %d)\n",
            device->sample_rates->min, device->sample_rates->max, device->sample_rate_current);
    logInfo("  Latency: %0.8f - %0.8f (Currently %0.8f)\n",
            device->software_latency_min, device->software_latency_max,
            device->software_latency_current);

    bool monoSupported = false;
    bool stereoSupported = false;
    for(int layoutIndex=0; layoutIndex<device->layout_count; layoutIndex++)
    {
        if(device->layouts[layoutIndex].channel_count == 1)
            monoSupported = true;
        if(device->layouts[layoutIndex].channel_count == 2)
            stereoSupported = true;
    }
    const char* monoSupportStr = monoSupported ? "Available" : "Unavailable";
    const char* stereoSupportStr = stereoSupported ? "Available" : "Unavailable";
    logInfo("  %d layouts supported. Mono support: %s. Stereo support: %s\n", device->layout_count, monoSupportStr, stereoSupportStr);

    bool float32Supported = false;
    for(int formatIndex=0; formatIndex<device->format_count; formatIndex++)
    {
        if(device->formats[formatIndex] == SoundIoFormatFloat32NE)
        {
            float32Supported = true;
        }
    }
    const char* floatSupportStr = float32Supported ? "Available" : "Unavailable";
    logInfo("  %d formats supported. Float32 support: %s\n", device->format_count, floatSupportStr);
}

static void inReadCallback(SoundIoInStream* stream, int frameCountMin, int frameCountMax)
{
    // TODO: If we take (frameCountMin+frameCountMax)/2 then we seem to get called WAY too often
    //       and get random values, should probably check the error and overflow callbacks
    int framesRemaining = frameCountMax;//frameCountMin + (frameCountMax-frameCountMin)/2;
    //logTerm("Read callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    SoundIoChannelArea* inArea;

    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int readError = soundio_instream_begin_read(stream, &inArea, &frameCount);
        if(readError)
        {
            logWarn("Read error: %s\n", soundio_strerror(readError));
            break;
        }

        for(int frame=0; frame<frameCount; ++frame)
        {
            // NOTE: We assume here that our input stream is mono.
            float val = *((float*)(inArea[0].ptr));
            inArea[0].ptr += inArea[0].step;

            inBuffer->write(val);
        }

        if(frameCount > 0)
        {
            soundio_instream_end_read(stream);
        }
        framesRemaining -= frameCount;
    }
}

static void outWriteCallback(SoundIoOutStream* stream, int frameCountMin, int frameCountMax)
{
    int samplesPerFrame = (AUDIO_PACKET_DURATION_MS*stream->sample_rate)/1000;
    int framesRemaining = clamp(samplesPerFrame, frameCountMin, frameCountMax);
    //logTerm("Write callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    int channelCount = stream->layout.channel_count;
    SoundIoChannelArea* outArea;

    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int writeError = soundio_outstream_begin_write(stream, &outArea, &frameCount);
        if(writeError)
        {
            logWarn("Write error: %s\n", soundio_strerror(writeError));
            break;
        }

        for(int frame=0; frame<frameCount; ++frame)
        {
            // TODO: Proper audio mixing. Reading: http://www.voegler.eu/pub/audio/digital-audio-mixing-and-normalization.html
            float val = 0.0f;
            if(audioState.isListeningToInput)
            {
                // NOTE: This will not modify val if there is no data available in listenBuffer.
                listenBuffer->read(&val);
            }

            for(auto userKV : audioUsers)
            {
                UserAudioData& user = userKV.second;

                float temp;
                user.buffer->read(&temp);
                val += temp;
            }
            for(int sourceIndex=0; sourceIndex<sourceList.size(); sourceIndex++)
            {
                // NOTE: This will not modify temp if there is no data available in listenBuffer.
                float temp = 0.0f;
                sourceList[sourceIndex]->read(&temp);
                val += temp;
            }

            for(int channel=0; channel<channelCount; ++channel)
            {
                float* samplePtr = (float*)outArea[channel].ptr;
                *samplePtr = val;
                outArea[channel].ptr += outArea[channel].step;
            }
        }

        soundio_outstream_end_write(stream);
        framesRemaining -= frameCount;
    }
}

static void inOverflowCallback(SoundIoInStream* stream)
{
    logWarn("Input overflow on stream from %s\n", stream->device->name);
}

static void outUnderflowCallback(SoundIoOutStream* stream)
{
    logTerm("Output underflow on stream to %s\n", stream->device->name);
}

static void inErrorCallback(SoundIoInStream* stream, int error)
{
    logWarn("Input error on stream from %s: %s\n", stream->device->name, soundio_strerror(error));
}

static void outErrorCallback(SoundIoOutStream* stream, int error)
{
    logWarn("Output error on stream from %s: %s\n", stream->device->name, soundio_strerror(error));
}

static void decodeSingleFrame(OpusDecoder* decoder,
                              int sourceLength, uint8_t* sourceBufferPtr,
                              Audio::AudioBuffer& targetAudioBuffer)
{
    assert(targetAudioBuffer.Capacity >= AUDIO_PACKET_FRAME_SIZE);
    assert(targetAudioBuffer.SampleRate == Audio::NETWORK_SAMPLE_RATE);

    int packetLength = sourceLength;
    int correctErrors = 0; // TODO: What value do we actually want here?
    int framesDecoded = opus_decode_float(decoder,
                                          sourceBufferPtr, packetLength,
                                          targetAudioBuffer.Data, targetAudioBuffer.Capacity,
                                          correctErrors);
    targetAudioBuffer.Length = framesDecoded;

    if(framesDecoded < 0)
    {
        logWarn("Error decoding audio data. Error %d\n", framesDecoded);
    }
    else if(framesDecoded > 0 && sourceBufferPtr == nullptr)
    {
        logWarn("We got %d frames from PLC!\n", framesDecoded);
    }
}

static int encodeSingleFrame(Audio::AudioBuffer& sourceBuffer, int targetLength, uint8_t* targetBufferPtr)
{
    assert(sourceBuffer.SampleRate == Audio::NETWORK_SAMPLE_RATE);
    assert(sourceBuffer.Length == AUDIO_PACKET_FRAME_SIZE);

    int outputLength = opus_encode_float(encoder,
                                         sourceBuffer.Data, AUDIO_PACKET_FRAME_SIZE,
                                         targetBufferPtr, targetLength);
    if(outputLength < 0)
    {
        logWarn("Error encoding audio. Error code %d\n", outputLength);
        return 0;
    }
    return outputLength;
}

void Audio::ListenToInput(bool listening)
{
    audioState.isListeningToInput = listening;
}

void Audio::GenerateToneInput(bool generateTone)
{
    audioState.generateToneInput = generateTone;
}

void Audio::PlayTestSound()
{
    // Create the test sound based on the sample rate that we got
    int sampleSoundSampleRate = outStream->sample_rate;
    int sampleSoundSampleCount = sampleSoundSampleRate;
    RingBuffer* sampleSource = new RingBuffer(sampleSoundSampleRate, sampleSoundSampleCount);

    float twopi = 2.0f*3.1415927f;
    float frequency = 261.6f; // Middle C
    float timestep = 1.0f/(float)outStream->sample_rate;
    float sampleTime = 0.0f;
    for(int sampleIndex=0; sampleIndex<sampleSoundSampleCount-1; sampleIndex++)
    {
        float sinVal = 0.0f;
        for(int i=0; i<4; i++)
        {
            sinVal += ((5-i)/(5.0f))*sinf(((1 << i)*frequency)*twopi*sampleTime);
        }

        sinVal *= 0.1f;
        sampleSource->write(sinVal);
        sampleTime += timestep;
    }

    sourceList.insert(sampleSource);
}

void Audio::AddAudioUser(UserIdentifier userId)
{
    logInfo("Added audio user with ID: %d\n", userId);
    int32 opusError;
    int32 channels = 1;
    UserAudioData newUser = {};
    newUser.decoder = opus_decoder_create(NETWORK_SAMPLE_RATE, channels, &opusError);
    logInfo("Opus decoder created: %d\n", opusError);

    newUser.buffer = new RingBuffer(outStream->sample_rate, RING_BUFFER_SIZE);
    newUser.jitter = new JitterBuffer();

    audioUsers[userId] = newUser;
}

void Audio::RemoveAudioUser(UserIdentifier userId)
{
    auto oldUserIter = audioUsers.find(userId);
    if(oldUserIter == audioUsers.end())
        return;

    UserAudioData& oldUser = oldUserIter->second;
    delete oldUser.jitter;
    if(oldUser.decoder)
    {
        opus_decoder_destroy(oldUser.decoder);
    }
    audioUsers.erase(userId);
}

void Audio::ProcessIncomingPacket(NetworkAudioPacket& packet)
{
    auto srcUserIter = audioUsers.find(packet.srcUser);
    if(srcUserIter == audioUsers.end())
    {
        logWarn("Received an audio packet from unknown user ID %d\n", packet.srcUser);
        return;
    }

    UserAudioData& srcUser = srcUserIter->second;
    logFile("Received audio packet %d for user %d\n", packet.index, packet.srcUser);

    srcUser.jitter->Add(packet.index, packet.encodedDataLength, packet.encodedData);
}

bool Audio::enableMicrophone(bool enabled)
{
    // TODO: Apparently some backends (e.g JACK) don't support pausing at all
    const char* toggleString = enabled ? "Enable" : "Disable";
    bool result = enabled;
    if(inStream)
    {
        logInfo("%s audio input\n", toggleString);
        int error = soundio_instream_pause(inStream, !enabled);
        if(error != SoundIoErrorNone)
        {
            logWarn("Error toggling microhpone: %s\n", soundio_strerror(error));
            result = false; // TODO: libsoundio doesn't let us query the current state of the stream
        }
    }
    else
    {
        logWarn("%s audio input failed: No open input stream\n", toggleString);
        result = false;
    }

    audioState.inputEnabled = result;
    return result;
}

bool Audio::enableSpeakers(bool enabled)
{
    // TODO: Apparently some backends (e.g JACK) don't support pausing at all
    const char* toggleString = enabled ? "Enable" : "Disable";
    if(outStream)
    {
        logInfo("%s audio output\n", toggleString);
        int error = soundio_outstream_pause(outStream, !enabled);
        if(error != SoundIoErrorNone)
        {
            logWarn("Error toggling speakers: %s\n", soundio_strerror(error));
            return false;
        }
        return enabled;
    }
    else
    {
        logWarn("%s audio output failed: No open input stream\n", toggleString);
        return false;
    }
}

int Audio::GetAudioInputDevice()
{
    return audioState.currentInputDevice;
}

bool Audio::SetAudioInputDevice(int newInputDevice)
{
    // TODO: We should probably wait for any currently running callbacks to finish
    if(inStream)
    {
        soundio_instream_destroy(inStream);
    }

    audioState.currentInputDevice = newInputDevice;
    inDevice = audioState.inputDeviceList[audioState.currentInputDevice];
    inStream = soundio_instream_create(inDevice);
    inStream->read_callback = inReadCallback;
    inStream->overflow_callback = inOverflowCallback;
    inStream->error_callback = inErrorCallback;
    inStream->sample_rate = soundio_device_nearest_sample_rate(inDevice, NETWORK_SAMPLE_RATE);
    inStream->format = SoundIoFormatFloat32NE;
    inStream->layout = inDevice->layouts[0]; // NOTE: Devices are guaranteed to have at least 1 layout
    // TODO: We might well want to sort this first, to make a slightly more intelligent selection of the layout
    inStream->software_latency = 0.01f;
    // NOTE: Lower latency corresponds to higher CPU usage, at 0.001 or 0s libsoundio eats an entire CPU but at 0.005 its fine
    // TODO: We probably want to check to make sure we don't set this to a value lower than the
    //       minimum latency for the current device

    logInfo("Attempting to open audio input stream on device: %s\n", inDevice->name);
    logInfo("  Sample Rate: %d\n", inStream->sample_rate);
    logInfo("  Latency: %0.8f\n", inStream->software_latency);
    logInfo("  Layout: %s\n", inStream->layout.name);
    logInfo("  Format: %s\n", soundio_format_string(inStream->format));

    int openError = soundio_instream_open(inStream);
    if(openError != SoundIoErrorNone)
    {
        logFail("Error opening input stream: %s\n", soundio_strerror(openError));
        soundio_instream_destroy(inStream);
        inStream = 0;
        return false;
    }
    if(inStream->layout_error)
    {
        logFail("Input stream layout error: %s\n", soundio_strerror(inStream->layout_error));
    }

    int startError = soundio_instream_start(inStream);
    if(startError != SoundIoErrorNone)
    {
        logFail("Error starting input stream: %s\n", soundio_strerror(startError));
        soundio_instream_destroy(inStream);
        inStream = 0;
        return false;
    }

    logInfo("Successfully opened audio input stream on device: %s\n", inDevice->name);
    logInfo("  Sample Rate: %d\n", inStream->sample_rate);
    logInfo("  Latency: %0.8f\n", inStream->software_latency);
    logInfo("  Layout: %s\n", inStream->layout.name);
    logInfo("  Format: %s\n", soundio_format_string(inStream->format));

    inBuffer->sampleRate = inStream->sample_rate;

    return true;
}

int Audio::GetAudioOutputDevice()
{
    return audioState.currentOutputDevice;
}

bool Audio::SetAudioOutputDevice(int newOutputDevice)
{
    // TODO: We should probably wait for any currently running callbacks to finish
    if(outStream)
    {
        soundio_outstream_destroy(outStream);
    }

    audioState.currentOutputDevice = newOutputDevice;
    outDevice = audioState.outputDeviceList[audioState.currentOutputDevice];
    outStream = soundio_outstream_create(outDevice);
    outStream->write_callback = outWriteCallback;
    outStream->underflow_callback = outUnderflowCallback;
    outStream->error_callback = outErrorCallback;
    outStream->sample_rate = soundio_device_nearest_sample_rate(outDevice, NETWORK_SAMPLE_RATE);
    outStream->format = SoundIoFormatFloat32NE;
    outStream->layout = outDevice->layouts[0]; // NOTE: Devices are guaranteed to have at least 1 layout
    // TODO: We might well want to sort this first, to make a slightly more intelligent selection of the layout
    outStream->software_latency = 0.01f;
    // TODO: We probably want to check to make sure we don't set this to a value lower than the
    //       minimum latency for the current device

    logInfo("Attempting to open audio output stream on device: %s\n", outDevice->name);
    logInfo("  Sample Rate: %d\n", outStream->sample_rate);
    logInfo("  Latency: %0.8f\n", outStream->software_latency);
    logInfo("  Layout: %s\n", outStream->layout.name);
    logInfo("  Format: %s\n", soundio_format_string(outStream->format));

    int openError = soundio_outstream_open(outStream);
    if(openError != SoundIoErrorNone)
    {
        logFail("Error opening output stream: %s\n", soundio_strerror(openError));
        soundio_outstream_destroy(outStream);
        outStream = 0;
        return false;
    }
    if(outStream->layout_error)
    {
        logFail("Output stream layout error: %s\n", soundio_strerror(outStream->layout_error));
    }

    int startError = soundio_outstream_start(outStream);
    if(startError != SoundIoErrorNone)
    {
        logFail("Error starting output stream: %s\n", soundio_strerror(openError));
        soundio_outstream_destroy(outStream);
        outStream = 0;
        return false;
    }

    logInfo("Successfully opened audio output stream on device: %s\n", outDevice->name);
    logInfo("  Sample Rate: %d\n", outStream->sample_rate);
    logInfo("  Latency: %0.8f\n", outStream->software_latency);
    logInfo("  Layout: %s\n", outStream->layout.name);
    logInfo("  Format: %s\n", soundio_format_string(outStream->format));

    listenBuffer->sampleRate = outStream->sample_rate;
    for(auto& iter : audioUsers)
    {
        UserAudioData& user = iter.second;
        user.buffer->sampleRate = outStream->sample_rate;
    }

    return true;
}

static void backendDisconnectCallback(SoundIo* sio, int error)
{
    logWarn("SoundIo backend disconnected: %s\n", soundio_strerror(error));
    //NOTE: This assumes that we are only connected to a single backend, otherwise we
    //      would need to check that the devices belong to the disconnected backend
    if(error == SoundIoErrorBackendDisconnected)
    {
        if(audioState.inputDeviceList)
        {
            for(int i=0; i<audioState.inputDeviceCount; i++)
            {
                soundio_device_unref(audioState.inputDeviceList[i]);
            }
            delete[] audioState.inputDeviceList;
            audioState.inputDeviceList = NULL;
        }
        if(audioState.inputDeviceNames)
        {
            delete[] audioState.inputDeviceNames;
            audioState.inputDeviceNames = NULL;
        }
        audioState.inputDeviceCount = 0;
        audioState.currentInputDevice = 0;

        if(audioState.outputDeviceList)
        {
            for(int i=0; i<audioState.outputDeviceCount; i++)
            {
                soundio_device_unref(audioState.outputDeviceList[i]);
            }
            delete[] audioState.outputDeviceList;
            audioState.outputDeviceList = 0;
        }
        if(audioState.outputDeviceNames)
        {
            delete[] audioState.outputDeviceNames;
            audioState.outputDeviceNames = 0;
        }
        audioState.outputDeviceCount = 0;
        audioState.currentOutputDevice = 0;
    }
}

static void devicesChangeCallback(SoundIo* sio)
{
    // TODO: Check that this works correctly now with PulseAudio which (on my laptop) calls this
    //       *very* frequently, it should at least not try re-opening a stream each time now
    int inputDeviceCount = soundio_input_device_count(sio);
    int outputDeviceCount = soundio_output_device_count(sio);
    logInfo("SoundIo device list updated - %d input, %d output devices\n",
            inputDeviceCount, outputDeviceCount);

    logInfo("Setup audio input\n");
    // Store the ID of the current open input device, if any
    int currentInputDeviceNewIndex = -1;
    char* currentInputId = 0;
    if(audioState.currentInputDevice >= 0)
    {
        currentInputId = audioState.inputDeviceList[audioState.currentInputDevice]->id;
    }

    // Store input device information. Names are stored for use by ImGui
    int defaultInputDevice = soundio_default_input_device_index(sio);
    int managedInputDeviceCount = 0;
    for(int i=0; i<inputDeviceCount; i++)
    {
        SoundIoDevice* device = soundio_get_input_device(sio, i);
        bool isDefault = (i == defaultInputDevice);
        if(!device->probe_error)
        {
            if(currentInputId && (currentInputDeviceNewIndex == -1))
            {
                if(strcmp(device->id, currentInputId) == 0)
                {
                    currentInputDeviceNewIndex = i;
                }
            }
            if(!device->is_raw)
            {
                managedInputDeviceCount += 1;
                printDevice(device, isDefault);
            }
        }
        soundio_device_unref(device);
    }

    if(audioState.inputDeviceList)
    {
        for(int i=0; i<audioState.inputDeviceCount; i++)
        {
            soundio_device_unref(audioState.inputDeviceList[i]);
        }
        delete[] audioState.inputDeviceList;
    }
    if(audioState.inputDeviceNames)
        delete[] audioState.inputDeviceNames;
    audioState.inputDeviceList = new SoundIoDevice*[managedInputDeviceCount];
    audioState.inputDeviceNames = new char*[managedInputDeviceCount];
    audioState.inputDeviceCount = managedInputDeviceCount;
    audioState.currentInputDevice = -1;
    // TODO: Just store the list of valid device indices.
    //       As it stands we can still encounter problems if we get a probe error in the 1nd loop
    //       but not the 2nd because we'll allocate too little memory and overflow the buffer.
    //       The same applies to output devices.
    int targetInputDeviceIndex = -1;
    int managedInputIndex = 0;
    int needNewInputStream = true;
    for(int i=0; i<inputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_input_device(sio, i);
        if(!device->probe_error && !device->is_raw)
        {
            audioState.inputDeviceList[managedInputIndex] = device;
            audioState.inputDeviceNames[managedInputIndex] = device->name;
            // NOTE: We use the current open device if it exists, otherwise we'll use the default
            if(targetInputDeviceIndex < 0)
            {
                if(i == currentInputDeviceNewIndex)
                {
                    audioState.currentInputDevice = managedInputIndex;
                    needNewInputStream = false;
                }
                else if(i == defaultInputDevice)
                {
                    targetInputDeviceIndex = managedInputIndex;
                }
            }
            managedInputIndex += 1;
        }
        else
        {
            soundio_device_unref(device);
        }
    }

    if(needNewInputStream)
    {
        // NOTE: If there are no input devices then target will be -1 even if needNew is true
        if(targetInputDeviceIndex >= 0)
        {
            if(!Audio::SetAudioInputDevice(targetInputDeviceIndex))
            {
                logWarn("Error: Unable to initialize audio input device %s\n",
                        audioState.inputDeviceNames[targetInputDeviceIndex]);
            }
        }
        else
        {
            // TODO: We probably want to just open SOME device
            logWarn("Error: No non-raw default audio input device\n");
        }
    }

    logInfo("Setup audio output\n");
    // Store the ID of the current open output device, if any
    int currentOutputDeviceNewIndex = -1;
    char* currentOutputId = 0;
    if(audioState.currentOutputDevice >= 0)
    {
        currentOutputId = audioState.outputDeviceList[audioState.currentOutputDevice]->id;
    }

    // Store input device information. Names are stored for use by ImGui
    int defaultOutputDevice = soundio_default_output_device_index(sio);
    int managedOutputDeviceCount = 0;
    for(int i=0; i<outputDeviceCount; i++)
    {
        SoundIoDevice* device = soundio_get_output_device(sio, i);
        bool isDefault = (i == defaultOutputDevice);
        if(!device->probe_error)
        {
            if(currentOutputId && (currentOutputDeviceNewIndex == -1))
            {
                if(strcmp(device->id, currentOutputId) == 0)
                {
                    currentOutputDeviceNewIndex = i;
                }
            }
            if(!device->is_raw)
            {
                managedOutputDeviceCount += 1;
                printDevice(device, isDefault);
            }
        }
        soundio_device_unref(device);
    }

    if(audioState.outputDeviceList)
    {
        for(int i=0; i<audioState.outputDeviceCount; i++)
        {
            soundio_device_unref(audioState.outputDeviceList[i]);
        }
        delete[] audioState.outputDeviceList;
    }
    if(audioState.outputDeviceNames)
        delete[] audioState.outputDeviceNames;
    audioState.outputDeviceList = new SoundIoDevice*[managedOutputDeviceCount];
    audioState.outputDeviceNames = new char*[managedOutputDeviceCount];
    audioState.outputDeviceCount = managedOutputDeviceCount;
    audioState.currentOutputDevice = -1;
    int targetOutputDeviceIndex = -1;
    int managedOutputIndex = 0;
    int needNewOutputStream = true;
    for(int i=0; i<outputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_output_device(sio, i);
        if(!device->probe_error && !device->is_raw)
        {
            audioState.outputDeviceList[managedOutputIndex] = device;
            audioState.outputDeviceNames[managedOutputIndex] = device->name;
            // NOTE: We use the current open device if it exists, otherwise we'll use the default
            if(targetOutputDeviceIndex == -1)
            {
                if(i == currentOutputDeviceNewIndex)
                {
                    audioState.currentOutputDevice = managedOutputIndex;
                    needNewOutputStream = false;
                }
                if(i == defaultOutputDevice)
                {
                    targetOutputDeviceIndex = managedOutputIndex;
                }
            }
            managedOutputIndex += 1;
        }
        else
        {
            soundio_device_unref(device);
        }
    }

    if(needNewOutputStream)
    {
        if(targetOutputDeviceIndex >= 0)
        {
            if(!Audio::SetAudioOutputDevice(targetOutputDeviceIndex))
            {
                logWarn("Error: Unable to initialize audio output device %s\n",
                        audioState.outputDeviceNames[targetOutputDeviceIndex]);
            }
        }
        else
        {
            // TODO: We probably want to just open SOME device
            logWarn("Error: No non-raw default audio output device\n");
        }
    }
}

bool Audio::Setup()
{
    logInfo("Initializing %s\n", opus_get_version_string());
    int opusError;
    int opusChannels = 1;
    int opusApplication = OPUS_APPLICATION_VOIP;
    encoder = opus_encoder_create(NETWORK_SAMPLE_RATE, opusChannels, opusApplication, &opusError);
    logInfo("Opus Error from encoder creation: %d\n", opusError);

    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(6));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(encoder, OPUS_SET_APPLICATION(OPUS_APPLICATION_VOIP));
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(5));

    opus_int32 complexity;
    opus_int32 bitrate;
    opus_encoder_ctl(encoder, OPUS_GET_COMPLEXITY(&complexity));
    opus_encoder_ctl(encoder, OPUS_GET_BITRATE(&bitrate));
    logInfo("Complexity=%d, Bitrate=%d\n", complexity, bitrate);

    audioState.encodingBuffer = AudioBuffer(2880);
    audioState.encodingBuffer.SampleRate = NETWORK_SAMPLE_RATE;

    audioState.decodingBuffer = AudioBuffer(2880);
    audioState.decodingBuffer.SampleRate = NETWORK_SAMPLE_RATE;

    // Initialize the current devices to null so that we will connect automatically when we 
    // get a list of connected devices
    audioState.currentInputDevice = -1;
    audioState.currentOutputDevice = -1;

    inBuffer = new RingBuffer(NETWORK_SAMPLE_RATE, RING_BUFFER_SIZE);
    listenBuffer = new RingBuffer(1, RING_BUFFER_SIZE);

    micBuffer = Audio::AudioBuffer(AUDIO_PACKET_FRAME_SIZE);
    micBuffer.SampleRate = NETWORK_SAMPLE_RATE;
    presendBuffer = new RingBuffer(NETWORK_SAMPLE_RATE, RING_BUFFER_SIZE);

    logInfo("Initializing libsoundio %s\n", soundio_version_string());
    soundio = soundio_create();
    if(!soundio)
    {
        logFail("Unable to create libsoundio context\n");
        return false;
    }
    soundio->on_devices_change = devicesChangeCallback;
    soundio->on_backend_disconnect = backendDisconnectCallback;
    int backendCount = soundio_backend_count(soundio);
    logInfo("%d audio backends are available:\n", backendCount);
    for(int backendIndex=0; backendIndex<backendCount; backendIndex++)
    {
        logInfo("  %s\n", soundio_backend_name(soundio_get_backend(soundio, backendIndex)));
    }

    int connectError = soundio_connect(soundio);
    if(connectError)
    {
        logFail("Unable to connect to libsoundio backend %s: %s\n",
                soundio_backend_name(soundio->current_backend), soundio_strerror(connectError));
        soundio_destroy(soundio);
        return false;
    }
    logInfo("Backend %s connected\n", soundio_backend_name(soundio->current_backend));
    soundio_flush_events(soundio);
    logInfo("SoundIO event queue flushed\n");
    // TODO: Check the supported input/output formats

    return true;
}

static Audio::NetworkAudioPacket* CreateOutputPacket(Audio::AudioBuffer& sourceBuffer)
{
    assert(sourceBuffer.Length == AUDIO_PACKET_FRAME_SIZE);
    assert(sourceBuffer.SampleRate == Audio::NETWORK_SAMPLE_RATE);

    Audio::NetworkAudioPacket* audioPacket = new Audio::NetworkAudioPacket();
    audioPacket->srcUser = localUser->ID;

    int audioBytes = encodeSingleFrame(sourceBuffer, AUDIO_PACKET_FRAME_SIZE, audioPacket->encodedData);
    audioPacket->encodedDataLength = audioBytes;

    return audioPacket;
}

void Audio::SendAudioToUser(ClientUserData* user, NetworkAudioPacket* audioPacket)
{
    audioPacket->index = user->lastSentAudioPacket++;
    logFile("Send audio packet %d to user %d\n", audioPacket->index, user->ID);

    NetworkOutPacket outPacket = createNetworkOutPacket(NET_MSGTYPE_AUDIO);
    audioPacket->serialize(outPacket);

    outPacket.send(user->netPeer, 0, false);
}

static void ProduceASingleAudioOutputPacket()
{
    micBuffer.Length = 0;
    for(int i=0; i<AUDIO_PACKET_FRAME_SIZE; i++)
    {
        micBuffer.Length += presendBuffer->read(&micBuffer.Data[micBuffer.Length]);
    }
    assert(micBuffer.Length == AUDIO_PACKET_FRAME_SIZE);

    if(audioState.generateToneInput)
    {
        double twopi = 2.0*3.1415927;
        double frequency = 261.6; // Middle C
        double timestep = 1.0/micBuffer.SampleRate;
        static double sampleTime = 0.0;
        for(int sampleIndex=0; sampleIndex<micBuffer.Capacity; sampleIndex++)
        {
            double sinVal = 0.05 * sin(frequency*twopi*sampleTime);
            micBuffer.Data[sampleIndex] = (float)sinVal;
            sampleTime += timestep;
        }
    }

    float rms = ComputeRMS(micBuffer);
    audioState.currentBufferVolume = rms;
    switch(audioState.inputActivationMode)
    {
        case Audio::MicActivationMode::Always:
            audioState.inputActive = true;
            break; // Active is already true

        case Audio::MicActivationMode::PushToTalk:
            audioState.inputActive = Platform::IsPushToTalkKeyPushed();
            break;

        case Audio::MicActivationMode::Automatic:
            audioState.inputActive = (rms >= 0.1f);
            break;

        default:
            logWarn("Unrecognized audio input activation mode: %d\n",
                    audioState.inputActivationMode);
    }

    bool anEntirePacketIsAvailable = (micBuffer.Length == AUDIO_PACKET_FRAME_SIZE);
    if(anEntirePacketIsAvailable)
    {
        if(audioState.isListeningToInput)
        {
            resampleBuffer2Ring(audioState.inputListenResampler, micBuffer, *listenBuffer);
        }

        if(audioState.inputActive && Network::IsConnectedToMasterServer())
        {
            Audio::NetworkAudioPacket* audioPacket = CreateOutputPacket(micBuffer);
            for(int i=0; i<remoteUsers.size(); i++)
            {
                ClientUserData* destinationUser = remoteUsers[i];
                Audio::SendAudioToUser(destinationUser, audioPacket);
            }
            delete audioPacket;
        }

        micBuffer.Length = 0;
    }
    else
    {
        logInfo("Insufficient data for sending an entire packet: %d of %d\n", micBuffer.Length, AUDIO_PACKET_FRAME_SIZE);
    }
}

float Audio::GetPacketLoss()
{
    uint64_t total = 0;
    uint64_t lost = 0;
    for(auto& iter : audioUsers)
    {
        UserAudioData& user = iter.second;
        total += user.totalExpectedPackets;
        lost += user.lostPackets;
    }

    if(total == 0)
    {
        return 0.0f;
    }
    return lost * 1.0f/total;
}

void Audio::Update()
{
    soundio_flush_events(soundio);

    if(audioState.inputEnabled)
    {
        // TODO: Rename the resampler to something that makes more sense.
        resampleRing2Ring(sendResampler, *inBuffer, *presendBuffer);

        int outputCount = 0;
        while(presendBuffer->count() >= AUDIO_PACKET_FRAME_SIZE)
        {
            ProduceASingleAudioOutputPacket();
            outputCount++;
        }
        logFile("Produced %d output packets this tick (%d samples remaining in presend)\n",
                outputCount, presendBuffer->count());
    }

    for(auto& iter : audioUsers)
    {
        UserAudioData& srcUser = iter.second;

        while(srcUser.buffer->count() <= 2*AUDIO_PACKET_FRAME_SIZE)
        {
            uint8_t* dataToDecode = nullptr;
            uint16_t dataToDecodeLen = srcUser.jitter->Get(&dataToDecode);
            AudioBuffer tempBuffer = {};
            tempBuffer.Capacity = AUDIO_PACKET_FRAME_SIZE;
            tempBuffer.Data = new float[tempBuffer.Capacity];
            tempBuffer.SampleRate = NETWORK_SAMPLE_RATE;

            if(srcUser.totalExpectedPackets >= 100)
            {
                srcUser.totalExpectedPackets /= 2;
                srcUser.lostPackets /= 2;
            }
            srcUser.totalExpectedPackets++;
            if(dataToDecodeLen == 0)
            {
                srcUser.lostPackets++;
            }

            decodeSingleFrame(srcUser.decoder,
                              dataToDecodeLen, dataToDecode,
                              tempBuffer);

            int bufferItemOffset = srcUser.jitter->ItemCount() - srcUser.jitter->DesiredItemCount();
            if(bufferItemOffset > 1) // We have more items than we would like, speed up
            {
                ResampleStreamContext speedResampler = srcUser.receiveResampler;
                double slowDown = -0.15;
                AudioBuffer longBuffer = {};
                longBuffer.Capacity = AUDIO_PACKET_FRAME_SIZE*5;
                longBuffer.Data = new float[longBuffer.Capacity];
                longBuffer.SampleRate = (int)(NETWORK_SAMPLE_RATE*(1+slowDown));
                resampleBuffer2Buffer(speedResampler, tempBuffer, longBuffer);
                longBuffer.SampleRate = NETWORK_SAMPLE_RATE;

                resampleBuffer2Ring(srcUser.receiveResampler, longBuffer, *srcUser.buffer);
                delete[] longBuffer.Data;
            }
            else if(bufferItemOffset < -1) // We have fewer items than we would like, slow down
            {
                ResampleStreamContext speedResampler = srcUser.receiveResampler;
                double slowDown = 0.15;
                AudioBuffer longBuffer = {};
                longBuffer.Capacity = AUDIO_PACKET_FRAME_SIZE*5;
                longBuffer.Data = new float[longBuffer.Capacity];
                longBuffer.SampleRate = (int)(NETWORK_SAMPLE_RATE*(1+slowDown));
                resampleBuffer2Buffer(speedResampler, tempBuffer, longBuffer);
                longBuffer.SampleRate = NETWORK_SAMPLE_RATE;

                resampleBuffer2Ring(srcUser.receiveResampler, longBuffer, *srcUser.buffer);
                delete[] longBuffer.Data;
            }
            else // Just play it as-is
            {
                resampleBuffer2Ring(srcUser.receiveResampler, tempBuffer, *srcUser.buffer);
            }
            delete[] tempBuffer.Data;
        }
    }

    // NOTE: This technically could run while we're reading audio data from sourceList in the
    //       output callback, but that probably isn't a problem because it'd just mean that
    //       we skip one callback's worth of audio for a handful of sources.
    for(int sourceIndex=0; sourceIndex<sourceList.size(); sourceIndex++)
    {
        RingBuffer* src = sourceList[sourceIndex];
        if(src->count() == 0)
        {
            delete src;
            sourceList.removeAt(sourceIndex);
            sourceIndex--;
        }
    }
}

int Audio::InputDeviceCount()
{
    return audioState.inputDeviceCount;
}

const char** Audio::InputDeviceNames()
{
    return (const char**)audioState.inputDeviceNames;
}

int Audio::OutputDeviceCount()
{
    return audioState.outputDeviceCount;
}

const char** Audio::OutputDeviceNames()
{
    return (const char**)audioState.outputDeviceNames;
}

void Audio::Shutdown()
{
    logInfo("Deinitialize audio subsystem\n");
    // TODO: Should we check that the mutexes are free at the moment? IE that any callbacks that
    //       may have been in progress when we stopped running, have finished

    if(inStream)
    {
        soundio_instream_pause(inStream, true);
        soundio_instream_destroy(inStream);
    }
    if(inBuffer)
        delete inBuffer;
    for(int i=0; i<audioState.inputDeviceCount; ++i)
    {
        soundio_device_unref(audioState.inputDeviceList[i]);
    }

    if(outStream)
    {
        soundio_outstream_pause(outStream, true);
        soundio_outstream_destroy(outStream);
    }
    for(int i=0; i<audioState.outputDeviceCount; ++i)
    {
        soundio_device_unref(audioState.outputDeviceList[i]);
    }
    soundio_destroy(soundio);
    opus_encoder_destroy(encoder);

    sourceList.pointerClear();
}

float Audio::ComputeRMS(AudioBuffer& buffer)
{
    float result = 0.0f;
    for(int i=0; i<buffer.Length; ++i)
    {
        result += (buffer.Data[i]*buffer.Data[i])/buffer.Length;
    }
    return sqrtf(result);
}

bool Audio::IsMicrophoneActive()
{
    return audioState.inputActive;
}

float Audio::GetInputVolume()
{
    return audioState.currentBufferVolume;
}

Audio::AudioBuffer::AudioBuffer(int initialCapacity)
{
    Data = new float[initialCapacity];
    Capacity = initialCapacity;
    Length = 0;
}

Audio::AudioBuffer::~AudioBuffer()
{
    if(Data)
    {
        // TODO: We obviously can't do this if we're passing AudioBuffers arround by value
        // delete[] Data;
    }
}

template<typename Packet>
bool Audio::NetworkAudioPacket::serialize(Packet& packet)
{
    packet.serializeuint16(this->srcUser);
    packet.serializeuint16(this->index);
    packet.serializeuint16(this->encodedDataLength);
    packet.serializebytes(this->encodedData, this->encodedDataLength);

    return true;
}
template bool Audio::NetworkAudioPacket::serialize(NetworkInPacket& packet);
template bool Audio::NetworkAudioPacket::serialize(NetworkOutPacket& packet);
