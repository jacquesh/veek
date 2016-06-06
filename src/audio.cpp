#include "audio.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "common.h"
#include "logging.h"
#include "platform.h"
#include "vecmath.h"
#include "ringbuffer.h"
#include "unorderedlist.h"

AudioData audioState = {};

static SoundIo* soundio = 0;
static OpusEncoder* encoder = 0;
static OpusDecoder* decoder = 0;

static SoundIoDevice* inDevice = 0;
static SoundIoInStream* inStream = 0;
static RingBuffer* inBuffer = 0; // TODO: These should probably be static but at the moment we
                                 //       use them for the microphone volume bars

static SoundIoDevice* outDevice = 0;
static SoundIoOutStream* outStream = 0;

static UnorderedList<AudioSource> sourceList(10); // TODO: Pick a correct max value here, 8 users + test sound + listening? I dunno

static RingBuffer* listenBuffer;
static RingBuffer* userBuffers[MAX_USERS];

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

void inReadCallback(SoundIoInStream* stream, int frameCountMin, int frameCountMax)
{
    // NOTE: We assume all audio input is MONO, which should always we the case if we didn't
    //       get an error during initialization since we specifically ask for MONO
    int channelCount = stream->layout.channel_count;

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
            // NOTE: We assume here that our input stream is MONO, see assertions above
            float val = *((float*)(inArea[0].ptr));
            inArea[0].ptr += inArea[0].step;

            inBuffer->write(1, &val);

            if(audioState.isListeningToInput)
            {
                listenBuffer->write(1, &val);
            }
        }

        soundio_instream_end_read(stream);
        framesRemaining -= frameCount;
    }
}

void outWriteCallback(SoundIoOutStream* stream, int frameCountMin, int frameCountMax)
{
    int framesRemaining = frameCountMin;
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
            float val = 0.0f;
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

void inOverflowCallback(SoundIoInStream* stream)
{
    logWarn("Input overflow!\n");
}

void outUnderflowCallback(SoundIoOutStream* stream)
{
    logTerm("Output underflow!\n");
}

void inErrorCallback(SoundIoInStream* stream, int error)
{
    logWarn("Input error: %s\n", soundio_strerror(error));
}

void outErrorCallback(SoundIoOutStream* stream, int error)
{
    logWarn("Output error: %s\n", soundio_strerror(error));
}

void listenToInput(bool listening)
{
    audioState.isListeningToInput = listening;
}

void playTestSound()
{
    // TODO
}

int decodePacket(int sourceLength, uint8_t* sourceBufferPtr,
                  int targetLength, float* targetBufferPtr)
{
    int frameSize = 240;
    uint8_t* sourceBuffer = sourceBufferPtr;
    float* targetBuffer = targetBufferPtr;
    int sourceLengthRemaining = sourceLength;
    int targetLengthRemaining = targetLength;

    while((sourceLengthRemaining >= sizeof(int)) && (targetLengthRemaining >= frameSize))
    {
        int packetLength = *((int*)sourceBuffer); // TODO: Byte order checking/swapping

        int correctErrors = 0; // TODO
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
    }

    int framesWritten = targetLength - targetLengthRemaining;
    return framesWritten;
}

int encodePacket(int sourceLength, float* sourceBufferPtr,
                  int targetLength, uint8_t* targetBufferPtr)
{
    // TODO: Opus can only create packets from a given set of sample counts, we should probably
    //       try support other packet sizes (is larger better? can it be dynamic? are we always
    //       going to have a sample rate of 48k? etc)
    int frameSize = 240;
    float* sourceBuffer = sourceBufferPtr;
    uint8_t* targetBuffer = targetBufferPtr;
    int sourceLengthRemaining = sourceLength;
    int targetLengthRemaining = targetLength;

    // TODO: What is an appropriate minimum targetLengthRemaining relative to framesize?
    while((sourceLengthRemaining >= frameSize) && (targetLengthRemaining >= frameSize))
    {
        // TODO: Multiple channels, this currently assumes a single channel
        int packetLength = opus_encode_float(encoder,
                                             sourceBuffer, frameSize,
                                             targetBuffer+4, targetLengthRemaining);
        *((int*)targetBuffer) = packetLength; // TODO: Byte order checking/swapping
        if(packetLength < 0)
        {
            logWarn("Error encoding audio. Error code %d\n", packetLength);
            break;
        }

        sourceLengthRemaining -= frameSize;
        sourceBuffer += frameSize;
        targetLengthRemaining -= packetLength+4;
        targetBuffer += packetLength+4;
    }

    int bytesWritten = targetLength - targetLengthRemaining;
    return bytesWritten;
}

int addUserAudioData(int userIndex, int sourceBufferLength, float* sourceBufferPtr)
{
    RingBuffer* outBuffer = userBuffers[userIndex];

    int samplesToWrite = sourceBufferLength;
    int ringBufferFreeSpace = outBuffer->free();
    if(samplesToWrite > ringBufferFreeSpace)
    {
        samplesToWrite = ringBufferFreeSpace;
    }
    //logTerm("Write %d samples\n", samplesToWrite);

    outBuffer->write(samplesToWrite, sourceBufferPtr);

    return samplesToWrite;
}

int readAudioInputBuffer(int targetBufferLength, float* targetBufferPtr)
{
    // NOTE: We don't need to take the number of channels into account here if we consider a "sample"
    //       to be a single sample from a single channel, but its important to note that that is what
    //       we're currently doing
    int samplesToWrite = targetBufferLength;
    int samplesAvailable = inBuffer->count();
    if(samplesAvailable < samplesToWrite)
        samplesToWrite = samplesAvailable;

    inBuffer->read(samplesToWrite, targetBufferPtr);

    return samplesToWrite;
}

bool enableMicrophone(bool enabled)
{
    // TODO: Apparently some backends (e.g JACK) don't support pausing at all
    const char* toggleString = enabled ? "Enable" : "Disable";
    if(inStream)
    {
        logInfo("%s audio input\n", toggleString);
        int error = soundio_instream_pause(inStream, !enabled);
        if(error)
        {
            logWarn("Error toggling microhpone\n");
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

bool setAudioInputDevice(int newInputDevice)
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
    inStream->sample_rate = soundio_device_nearest_sample_rate(inDevice, 48000);
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
    return true;
}

bool setAudioOutputDevice(int newOutputDevice)
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
    outStream->sample_rate = soundio_device_nearest_sample_rate(outDevice, 48000);
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

    // Create the test sound based on the sample rate that we got
    uint32 sampleSoundSampleCount = 1 << 16;
    AudioSource sampleSource = {0};
    sampleSource.buffer = new RingBuffer(sampleSoundSampleCount);

    float twopi = 2.0f*3.1415927f;
    float frequency = 261.6f; // Middle C
    float timestep = 1.0f/(float)outStream->sample_rate;
    float sampleTime = 0.0f;
    for(uint32 sampleIndex=0; sampleIndex<sampleSoundSampleCount-1; sampleIndex++)
    {
        float sinVal = sinf(frequency*twopi*sampleTime);
        sampleSource.buffer->write(1, &sinVal);
        sampleTime += timestep;
    }
    sourceList.insert(sampleSource);

    AudioSource listenSource = {0};
    listenBuffer = new RingBuffer(1 << 13);
    listenSource.buffer = listenBuffer;
    sourceList.insert(listenSource);

    for(int userIndex=0; userIndex<MAX_USERS; userIndex++)
    {
        AudioSource userSource = {0};
        userBuffers[userIndex] = new RingBuffer(1 << 13);
        userSource.buffer = userBuffers[userIndex];
        sourceList.insert(userSource);
    }

    return true;
}

void backendDisconnectCallback(SoundIo* sio, int error)
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

void devicesChangeCallback(SoundIo* sio)
{
    // TODO: This doesn't appear to get called when I unplug/plug in my microphone (realtek does notice though)
    int inputDeviceCount = soundio_input_device_count(sio);
    int outputDeviceCount = soundio_output_device_count(sio);
    logInfo("SoundIo device list updated - %d input, %d output devices\n",
            inputDeviceCount, outputDeviceCount);

    // TODO: Check if the current device is still enabled, if it isn't we need to nullify things,
    //       if it is we need to update currentInputDevice to be the correct new index.
    //       It would seem that the only way we have of comparing devices is by the device.id string

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
        if(currentInputId && (currentInputDeviceNewIndex == -1))
        {
            if(strcmp(device->id, currentInputId) == 0)
            {
                currentInputDeviceNewIndex = i;
            }
        }
        if(!device->is_raw)
        {
            bool isDefault = (i == defaultInputDevice);
            printDevice(device, isDefault);
            managedInputDeviceCount += 1;
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
    int targetInputDeviceIndex = -1;
    int managedInputIndex = 0;
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
                    targetInputDeviceIndex = managedInputIndex;
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

    if(targetInputDeviceIndex >= 0)
    {
        if(!setAudioInputDevice(targetInputDeviceIndex))
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
        if(currentOutputId && (currentOutputDeviceNewIndex == -1))
        {
            if(strcmp(device->id, currentOutputId) == 0)
            {
                currentOutputDeviceNewIndex = i;
            }
        }
        if(!device->is_raw)
        {
            bool isDefault = (i == defaultOutputDevice);
            printDevice(device, isDefault);
            managedOutputDeviceCount += 1;
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
    int targetOutputDeviceIndex = -1;
    int managedOutputIndex = 0;
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
                    targetOutputDeviceIndex = managedOutputIndex;
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

    if(targetOutputDeviceIndex >= 0)
    {
        if(!setAudioOutputDevice(targetOutputDeviceIndex))
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

bool initAudio()
{
    // Initialize the current devices to null so that we will connect automatically when we 
    // get a list of connected devices
    audioState.currentInputDevice = -1;
    audioState.currentOutputDevice = -1;
    inBuffer = new RingBuffer(4096);

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

    logInfo("Initializing %s\n", opus_get_version_string());
    int opusError;
    opus_int32 opusSampleRate = 48000;
    int opusChannels = 1;
    int opusApplication = OPUS_APPLICATION_VOIP;
    encoder = opus_encoder_create(opusSampleRate, opusChannels, opusApplication, &opusError);
    logInfo("Opus Error from encoder creation: %d\n", opusError);
    decoder = opus_decoder_create(opusSampleRate, opusChannels, &opusError);
    logInfo("Opus Error from decoder creation: %d\n", opusError);

    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(6));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(encoder, OPUS_SET_APPLICATION(OPUS_APPLICATION_VOIP));
    //opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));

    opus_int32 complexity;
    opus_int32 bitrate;
    opus_encoder_ctl(encoder, OPUS_GET_COMPLEXITY(&complexity));
    opus_encoder_ctl(encoder, OPUS_GET_BITRATE(&bitrate));
    logInfo("Complexity=%d, Bitrate=%d\n", complexity, bitrate);

    return true;
}

void updateAudio()
{
    soundio_flush_events(soundio);
}

void deinitAudio()
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
    opus_decoder_destroy(decoder);

    soundio_destroy(soundio);
}

// TODO: This is just here for testing, writing the audio recording to file
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Mmreg.h>
#pragma pack (push,1)
typedef struct
{
	char			szRIFF[4];
	long			lRIFFSize;
	char			szWave[4];
	char			szFmt[4];
	long			lFmtSize;
	WAVEFORMATEX	wfex;
	char			szData[4];
	long			lDataSize;
} WAVEHEADER;
#pragma pack (pop)

void writeAudioToFile(int dataLength, uint8_t* data)
{
    WAVEHEADER audioHeader = {};

    FILE* audioFile = fopen("out.wav", "wb");
    sprintf(audioHeader.szRIFF, "RIFF");
    audioHeader.lRIFFSize = 0;
    sprintf(audioHeader.szWave, "WAVE");
    sprintf(audioHeader.szFmt, "fmt ");
    audioHeader.lFmtSize = sizeof(WAVEFORMATEX);
    audioHeader.wfex.nChannels = 1;
    audioHeader.wfex.wBitsPerSample = 8;
    audioHeader.wfex.wFormatTag = WAVE_FORMAT_PCM;
    audioHeader.wfex.nSamplesPerSec = 48000;
    audioHeader.wfex.nBlockAlign = audioHeader.wfex.nChannels*audioHeader.wfex.wBitsPerSample/8;
    audioHeader.wfex.nAvgBytesPerSec = audioHeader.wfex.nSamplesPerSec * audioHeader.wfex.nBlockAlign;
    audioHeader.wfex.cbSize = 0;
    sprintf(audioHeader.szData, "data");
    audioHeader.lDataSize = 0;
    fwrite(&audioHeader, sizeof(WAVEHEADER), 1, audioFile);

    fwrite(data, dataLength, 1, audioFile);
    fseek(audioFile, 4, SEEK_SET);
    int size = dataLength + sizeof(WAVEHEADER) - 8;
    fwrite(&size, 4, 1, audioFile);
    fseek(audioFile, 42, SEEK_SET);
    fwrite(&dataLength, 4, 1, audioFile);
    fclose(audioFile);
}
