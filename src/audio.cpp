#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unordered_map>

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "audio.h"
#include "audio_resample.h"
#include "common.h"
#include "logging.h"
#include "math_utils.h"
#include "network.h"
#include "platform.h"
#include "ringbuffer.h"
#include "unorderedlist.h"
#include "user.h"
#include "user_client.h"

struct AudioSource
{
    RingBuffer* buffer;
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

    bool generateToneInput;
    bool isListeningToInput;
    ResampleStreamContext inputListenResampler;

    Audio::AudioBuffer encodingBuffer; // Used for resampling before encoding, when necessary
    Audio::AudioBuffer decodingBuffer; // Used for resampling after decoding, when necessary
};

struct UserAudioData
{
    uint8_t lastReceivedPacketIndex;

    int32 sampleRate;
    OpusDecoder* decoder;
    RingBuffer* buffer;
};

static AudioData audioState = {};

static SoundIo* soundio = 0;
static OpusEncoder* encoder = 0;

static SoundIoDevice* inDevice = 0;
static SoundIoInStream* inStream = 0;
static RingBuffer* inBuffer = 0;

static SoundIoDevice* outDevice = 0;
static SoundIoOutStream* outStream = 0;

static UnorderedList<AudioSource> sourceList(10); // TODO: Pick a correct max value here, 8 users + test sound + listening? I dunno

static RingBuffer* listenBuffer;

static std::unordered_map<UserIdentifier, UserAudioData> audioUsers;

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

    logInfo("  %d layouts supported:\n", device->layout_count);
    for(int layoutIndex=0; layoutIndex<device->layout_count; layoutIndex++)
    {
        logInfo("    %s\n", device->layouts[layoutIndex].name);
    }

    logInfo("  %d formats supported:\n", device->format_count);
    for(int formatIndex=0; formatIndex<device->format_count; formatIndex++)
    {
        logInfo("    %s\n", soundio_format_string(device->formats[formatIndex]));
    }
}

static void inReadCallback(SoundIoInStream* stream, int frameCountMin, int frameCountMax)
{
    // NOTE: We assume all audio input is MONO, which should always we the case if we didn't
    //       get an error during initialization since we specifically ask for MONO
    // TODO: If we take (frameCountMin+frameCountMax)/2 then we seem to get called WAY too often
    //       and get random values, should probably check the error and overflow callbacks
    int framesRemaining = frameCountMax;//frameCountMin + (frameCountMax-frameCountMin)/2;
    //logTerm("Read callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    SoundIoChannelArea* inArea;

    // TODO: Check the free space in inBuffer
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
            // NOTE: We assume here that our input stream is MONO
            float val = *((float*)(inArea[0].ptr));
            inArea[0].ptr += inArea[0].step;

            inBuffer->write(1, &val);
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
    // TODO: 0.025 is twice per frame, we should probably link that in to where we actually specify
    //       the tickrate
    int samplesPerFrame = (int)(0.025f*stream->sample_rate);
    int framesRemaining = clamp(samplesPerFrame, frameCountMin, frameCountMax);
    //logTerm("Write callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    int channelCount = stream->layout.channel_count;
    SoundIoChannelArea* outArea;
    // TODO: Wraithy always gets 0 frameCountMin and then framesRemaining gets 0 and it underflows

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
                listenBuffer->read(1, &val);
#if 0
            for(auto userKV : audioUsers)
            {
                if(userKV.second.buffer->count() != 0)
                {
                    logTerm("Callback samples for user %d: %d\n", userKV.first, userKV.second.buffer->count());
                }
                if(userKV.second.buffer->count() > 0)
                {
                    float temp;
                    userKV.second.buffer->read(1, &temp);
                    val += temp;
                }
            }
#endif
            for(int sourceIndex=0; sourceIndex<sourceList.size(); sourceIndex++)
            {
                if(sourceList[sourceIndex].buffer->count() > 0)
                {
                    float temp;
                    sourceList[sourceIndex].buffer->read(1, &temp);
                    val += temp;
                }
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
    uint32 sampleSoundSampleCount = 1 << 16;
    AudioSource sampleSource = {};
    sampleSource.buffer = new RingBuffer(sampleSoundSampleCount);

    float* tempbuffer = new float[sampleSoundSampleCount];
    float twopi = 2.0f*3.1415927f;
    float frequency = 261.6f; // Middle C
    float timestep = 1.0f/(float)outStream->sample_rate;
    float sampleTime = 0.0f;
    for(uint32 sampleIndex=0; sampleIndex<sampleSoundSampleCount-1; sampleIndex++)
    {
        float sinVal = 0.0f;
        for(int i=0; i<4; i++)
        {
            sinVal += ((5-i)/(5.0f))*sinf(((1 << i)*frequency)*twopi*sampleTime);
        }
        //sampleSource.buffer->write(1, &sinVal);
        tempbuffer[sampleIndex] = 0.1f*sinVal;
        sampleTime += timestep;
    }

    int32 opusError;
    int32 channels = 1;
    UserAudioData newUser = {};
    OpusDecoder* decoder = opus_decoder_create(NETWORK_SAMPLE_RATE, channels, &opusError);
    if(opusError != OPUS_OK)
    {
        logWarn("Failed to create Opus decoder: %d\n", opusError);
    }

    AudioBuffer sampleBuffer = {};
    sampleBuffer.Capacity = sampleSoundSampleCount;
    sampleBuffer.Length = sampleBuffer.Capacity;
    sampleBuffer.SampleRate = outStream->sample_rate;
    sampleBuffer.Data = tempbuffer;
    AudioBuffer outputBuffer = {};
    outputBuffer.Capacity = sampleSoundSampleCount;
    outputBuffer.Length = 0;
    outputBuffer.SampleRate = outStream->sample_rate;
    outputBuffer.Data = new float[sampleSoundSampleCount];

    int encSize = 1 << 16;
    uint8_t* encodedData = new uint8_t[encSize];
    int encodedLength = encodePacket(sampleBuffer, encSize, encodedData);
    decodePacket(decoder, encodedLength, encodedData, outputBuffer);
    delete[] encodedData;

    sampleSource.buffer->write(outputBuffer.Length, outputBuffer.Data);
    sourceList.insert(sampleSource);
    // TODO: This needs to be removed from the list of sources when it's finished playing

    delete[] outputBuffer.Data;
    opus_decoder_destroy(decoder);
}

void Audio::decodePacket(OpusDecoder* decoder,
                         int sourceLength, uint8_t* sourceBufferPtr,
                         AudioBuffer& targetAudioBuffer)
{
    int frameSize = 240;
    uint8_t* sourceBuffer = sourceBufferPtr;
    float* targetBuffer = targetAudioBuffer.Data;
    int sourceLengthRemaining = sourceLength;
    int targetLengthRemaining = targetAudioBuffer.Capacity;
    int totalFramesDecoded = 0;

    if(targetAudioBuffer.SampleRate != NETWORK_SAMPLE_RATE)
    {
        targetBuffer = audioState.decodingBuffer.Data;
        targetLengthRemaining = audioState.decodingBuffer.Capacity;
    }

    while((sourceLengthRemaining >= sizeof(int)) && (targetLengthRemaining >= frameSize))
    {
        int packetLength = *((int*)sourceBuffer); // TODO: Byte order checking/swapping
        static bool printLen = true;
        if(printLen)
        {
            logInfo("Decode packet with length %d\n", packetLength);
            printLen = false;
        }

        int correctErrors = 0; // TODO: What value do we actually want here?
        int framesDecoded = opus_decode_float(decoder,
                                              sourceBuffer+4, packetLength,
                                              targetBuffer, targetLengthRemaining,
                                              correctErrors);
        if(framesDecoded < 0)
        {
            logWarn("Error decoding audio data. Error %d\n", framesDecoded);
            break;
        }

        sourceLengthRemaining -= packetLength+4;
        sourceBuffer += packetLength+4;
        targetLengthRemaining -= framesDecoded;
        targetBuffer += framesDecoded;
        totalFramesDecoded += framesDecoded;
    }

    if(targetAudioBuffer.SampleRate != NETWORK_SAMPLE_RATE)
    {
        logTerm("Resample a buffer after decoding\n");
        static ResampleStreamContext ctx = {};
        ctx.InputSampleRate = audioState.decodingBuffer.SampleRate;
        ctx.OutputSampleRate = targetAudioBuffer.SampleRate;
        targetAudioBuffer.Length = 0;
        audioState.decodingBuffer.Length = totalFramesDecoded;

        for(int i=0; i<audioState.decodingBuffer.Length; i++)
        {
            resampleStreamInput(ctx, audioState.decodingBuffer.Data[i]);
            while(!resampleStreamRequiresInput(ctx))
            {
                float sample = resampleStreamOutput(ctx);
                targetAudioBuffer.Data[targetAudioBuffer.Length++] = sample;
            }

        }
    }
    else
    {
        targetAudioBuffer.Length = totalFramesDecoded;
    }
}

int Audio::encodePacket(AudioBuffer& sourceBuffer,
                        int targetLength, uint8_t* targetBufferPtr)
{
    uint8_t* targetData = targetBufferPtr;
    int targetLengthRemaining = targetLength;

    float* sourceData = sourceBuffer.Data;
    int sourceLengthRemaining = sourceBuffer.Length;
    if(sourceBuffer.SampleRate != NETWORK_SAMPLE_RATE)
    {
        static ResampleStreamContext ctx = {};
        ctx.InputSampleRate = sourceBuffer.SampleRate;
        ctx.OutputSampleRate = NETWORK_SAMPLE_RATE;
        audioState.encodingBuffer.Length = 0;
        for(int i=0; i<sourceBuffer.Length; i++)
        {
            resampleStreamInput(ctx, sourceBuffer.Data[i]);
            while(!resampleStreamRequiresInput(ctx))
            {
                float sample = resampleStreamOutput(ctx);
                audioState.encodingBuffer.Data[audioState.encodingBuffer.Length++] = sample;
            }
        }
        sourceData = audioState.encodingBuffer.Data;
        sourceLengthRemaining = audioState.encodingBuffer.Length;
    }

    // TODO: Opus can only create packets from a given set of sample counts, we should probably
    //       try support other packet sizes (is larger better? can it be dynamic? are we always
    //       going to have a sample rate of 48k? etc)
    int frameSize = 240;

    // TODO: What is an appropriate minimum targetLengthRemaining relative to framesize?
    while((sourceLengthRemaining >= frameSize) && (targetLengthRemaining >= frameSize))
    {
        // TODO: Multiple channels, this currently assumes a single channel
        int packetLength = opus_encode_float(encoder,
                                             sourceData, frameSize,
                                             targetData+4, targetLengthRemaining);
        *((int*)targetData) = packetLength; // TODO: Byte order checking/swapping
        if(packetLength < 0)
        {
            logWarn("Error encoding audio. Error code %d\n", packetLength);
            break;
        }

        sourceLengthRemaining -= frameSize;
        sourceData += frameSize;
        targetLengthRemaining -= packetLength+4;
        targetData += packetLength+4;
    }

    int bytesWritten = targetLength - targetLengthRemaining;
    return bytesWritten;
}

void Audio::AddAudioUser(UserIdentifier userId)
{
    logInfo("Added audio user with ID: %d\n", userId);
    int32 opusError;
    int32 channels = 1;
    UserAudioData newUser = {};
    // TODO: Don't create the decoder here, create it when we receive a packet and we know what the sample rate is
    newUser.decoder = opus_decoder_create(NETWORK_SAMPLE_RATE, channels, &opusError);
    logInfo("Opus decoder created: %d\n", opusError);

    newUser.buffer = new RingBuffer(8192);

    AudioSource userSource = {};
    userSource.buffer = newUser.buffer;
    sourceList.insert(userSource);

    audioUsers[userId] = newUser;
}

void Audio::RemoveAudioUser(UserIdentifier userId)
{
    auto oldUserIter = audioUsers.find(userId);
    if(oldUserIter == audioUsers.end())
        return;

    UserAudioData& oldUser = oldUserIter->second;
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
    if(srcUser.lastReceivedPacketIndex + 1 != packet.index)
    {
        logWarn("Dropped audio packets %d to %d (inclusive)\n",
                srcUser.lastReceivedPacketIndex+1, packet.index-1);
    }
    srcUser.lastReceivedPacketIndex = packet.index;

    AudioBuffer tempBuffer = {};
    tempBuffer.Capacity = 2400;
    tempBuffer.Data = new float[tempBuffer.Capacity];
    tempBuffer.SampleRate = NETWORK_SAMPLE_RATE; // TODO: srcUser.sampleRate

    decodePacket(srcUser.decoder,
                 packet.encodedDataLength, packet.encodedData,
                 tempBuffer);
    logTerm("Received %d samples\n", tempBuffer.Length);
    assert(tempBuffer.Length <= srcUser.buffer->free()); // TODO: This is probably not a sensible assert? We don't want this to happen but if it does we don't want to crash.
    srcUser.buffer->write(tempBuffer.Length, tempBuffer.Data);
    logTerm("New sample count for user %d: %d\n", packet.srcUser, srcUser.buffer->count());
    delete[] tempBuffer.Data;
}

void Audio::readAudioInputBuffer(AudioBuffer& buffer)
{
    // NOTE: We don't need to take the number of channels into account here if we consider a "sample"
    //       to be a single sample from a single channel, but its important to note that that is what
    //       we're currently doing
    int samplesToWrite = buffer.Capacity;
    int samplesAvailable = inBuffer->count();
    if(samplesAvailable < samplesToWrite)
        samplesToWrite = samplesAvailable;

    inBuffer->read(samplesToWrite, buffer.Data);
    buffer.Length = samplesToWrite;
    buffer.SampleRate = inDevice->sample_rate_current;

    if(audioState.generateToneInput)
    {
        double twopi = 2.0*3.1415927;
        double frequency = 261.6; // Middle C
        double timestep = 1.0/buffer.SampleRate;
        static double sampleTime = 0.0;
        for(int sampleIndex=0; sampleIndex<buffer.Length; sampleIndex++)
        {
            double sinVal = 0.05 * sin(frequency*twopi*sampleTime);
            buffer.Data[sampleIndex] = (float)sinVal;
            sampleTime += timestep;
        }
    }

    if(audioState.isListeningToInput)
    {
        static ResampleStreamContext ctx = {};
        ctx.InputSampleRate = buffer.SampleRate;
        ctx.OutputSampleRate = outStream->sample_rate;
        for(int i=0; i<samplesToWrite; i++)
        {
            float resampledSamples[3];
            int resampleCount = resampleStream(ctx, buffer.Data[i], resampledSamples, 3);

            for(int j=0; j<resampleCount; j++)
            {
                listenBuffer->write(1, &resampledSamples[j]);
            }
        }
    }
}

bool Audio::enableMicrophone(bool enabled)
{
    // TODO: Apparently some backends (e.g JACK) don't support pausing at all
    const char* toggleString = enabled ? "Enable" : "Disable";
    if(inStream)
    {
        logInfo("%s audio input\n", toggleString);
        int error = soundio_instream_pause(inStream, !enabled);
        if(error != SoundIoErrorNone)
        {
            logWarn("Error toggling microhpone: %s\n", soundio_strerror(error));
            return false; // TODO: libsoundio doesn't let us query the current state of the stream
        }
        return enabled;
    }
    else
    {
        logWarn("%s audio input failed: No open input stream\n", toggleString);
        return false;
    }
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
    inStream->software_latency = 0.005f;
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
        if(inStream->layout_error)
        {
            logFail("  Stream layout error: %s\n", soundio_strerror(inStream->layout_error));
        }
        soundio_instream_destroy(inStream);
        inStream = 0;
        return false;
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

    audioState.inputListenResampler.InputSampleRate = inStream->sample_rate;

    return true;
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
    outStream->software_latency = 0.005f;
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
        if(outStream->layout_error)
        {
            logFail("  Stream layout error: %s\n", soundio_strerror(outStream->layout_error));
        }
        soundio_outstream_destroy(outStream);
        outStream = 0;
        return false;
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

    audioState.inputListenResampler.OutputSampleRate = outStream->sample_rate;

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
        printDevice(device, isDefault);
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
    int targetInputDeviceIndex = -1;
    int managedInputIndex = 0;
    int needNewInputStream = true;
    for(int i=0; i<inputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_input_device(sio, i);
        if(!device->is_raw)
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
        printDevice(device, isDefault);
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
        if(!device->is_raw)
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
    //opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));

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
    inBuffer = new RingBuffer(4096);

    listenBuffer = new RingBuffer(1 << 13);

    // Initialize SoundIO
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

void Audio::SendAudioToUser(ClientUserData* user, AudioBuffer& sourceBuffer)
{
    int encodedBufferLength = sourceBuffer.Length;
    uint8* encodedBuffer = new uint8[encodedBufferLength];
    memset(encodedBuffer, 0, encodedBufferLength);
    int audioBytes = encodePacket(sourceBuffer, encodedBufferLength, encodedBuffer);

    NetworkAudioPacket audioPacket = {};
    audioPacket.srcUser = localUser.ID;
    audioPacket.index = user->lastSentAudioPacket++;
    audioPacket.encodedDataLength = audioBytes;
    memcpy(audioPacket.encodedData, encodedBuffer, audioBytes);

    delete[] encodedBuffer;

    NetworkOutPacket outPacket = createNetworkOutPacket(NET_MSGTYPE_AUDIO);
    audioPacket.serialize(outPacket);

    NetworkInPacket inPacketTest = {};
    inPacketTest.contents = outPacket.contents;
    inPacketTest.length = outPacket.length;
    uint8_t packetType;
    inPacketTest.serializeuint8(packetType);
    NetworkAudioPacket audioInPacket;
    audioInPacket.serialize(inPacketTest);

    outPacket.send(user->netPeer, 0, false);
}

void Audio::Update()
{
    soundio_flush_events(soundio);
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
    // TODO: Wraithy crashes inside this function
    // TODO: Should we check that the mutexes are free at the moment? IE that any callbacks that
    //       may have been in progress when we stopped running, have finished
    // TODO: Clean up the sourceList and its elements (in particular the ringbuffers should
    //       probably be freed here)

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

    opus_encoder_destroy(encoder);
    soundio_destroy(soundio);
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
    packet.serializeuint8(this->srcUser);
    packet.serializeuint8(this->index);
    packet.serializeuint16(this->encodedDataLength);
    packet.serializebytes(this->encodedData, this->encodedDataLength);

    return true;
}
template bool Audio::NetworkAudioPacket::serialize(NetworkInPacket& packet);
template bool Audio::NetworkAudioPacket::serialize(NetworkOutPacket& packet);
