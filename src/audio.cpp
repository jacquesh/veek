#include "audio.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "common.h"
#include "logging.h"
#include "platform.h"
#include "vecmath.h"
#include "ringbuffer.h"

// TODO: Allow runtime switching of the recording/playback devices.
// TODO: The decoder states that we need to call decode with empty values for every lost packet
//       We do actually keep track of time but we have yet to use that to check for packet loss
AudioData audioState = {};

static SoundIo* soundio = 0;
static OpusEncoder* encoder = 0;
static OpusDecoder* decoder = 0;

static SoundIoDevice* inDevice = 0;
static SoundIoInStream* inStream = 0;
RingBuffer* inBuffer = 0; // TODO: These should probably be static but at the moment we use them for
Mutex* audioInMutex = 0;  //       the microphone volume bars

static SoundIoDevice* outDevice = 0;
static SoundIoOutStream* outStream = 0;
RingBuffer* outBuffer = 0;
Mutex* audioOutMutex = 0;

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
    assert(channelCount == 1);

    // TODO: If we take (frameCountMin+frameCountMax)/2 then we seem to get called WAY too often
    //       and get random values, should probably check the error and overflow callbacks
    int framesRemaining = frameCountMax;//frameCountMin + (frameCountMax-frameCountMin)/2;
    //logTerm("Read callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    SoundIoChannelArea* inArea;

    // TODO: Check the free space in inBuffer
    lockMutex(audioInMutex);
    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int readError = soundio_instream_begin_read(stream, &inArea, &frameCount);
        if(readError)
        {
            logWarn("Read error\n");
            break;
        }

        for(int frame=0; frame<frameCount; ++frame)
        {
            // NOTE: We assume here that our input stream is MONO, see assertions above
            float val = *((float*)(inArea[0].ptr));
            inArea[0].ptr += inArea[0].step;

            inBuffer->write(1, &val);
            inBuffer->advanceWritePointer(1);
        }

        soundio_instream_end_read(stream);
        framesRemaining -= frameCount;
    }
    unlockMutex(audioInMutex);
}

void outWriteCallback(SoundIoOutStream* stream, int frameCountMin, int frameCountMax)
{
    lockMutex(audioOutMutex);
    int framesAvailable = outBuffer->count();
    int framesRemaining = clamp(framesAvailable, frameCountMin, frameCountMax);
    //logTerm("Write callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    int channelCount = stream->layout.channel_count;
    SoundIoChannelArea* outArea;

    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int writeError = soundio_outstream_begin_write(stream, &outArea, &frameCount);
        if(writeError)
        {
            logWarn("Write error\n");
            break;
        }

        RingBuffer* sourceBuffer = outBuffer;
        if(audioState.isListeningToInput)
        {
            sourceBuffer = inBuffer;
        }

        framesAvailable = sourceBuffer->count();
        framesAvailable = min(framesAvailable, frameCount);
        for(int frame=0; frame<framesAvailable; ++frame)
        {
            float val;
            sourceBuffer->read(1, &val);
            sourceBuffer->advanceReadPointer(1);

            for(int channel=0; channel<channelCount; ++channel)
            {
                float* samplePtr = (float*)outArea[channel].ptr;
                *samplePtr = val;
                outArea[channel].ptr += outArea[channel].step;
            }
        }

        for(int frame=framesAvailable; frame<frameCount; ++frame)
        {
            for(int channel=0; channel<channelCount; ++channel)
            {
                float* samplePtr = (float*)outArea[channel].ptr;
                *samplePtr = 0.0f;
                outArea[channel].ptr += outArea[channel].step;
            }
        }

        soundio_outstream_end_write(stream);
        framesRemaining -= frameCount;
    }
    unlockMutex(audioOutMutex);
}

void inOverflowCallback(SoundIoInStream* stream)
{
    logWarn("Input underflow!\n");
}

void outUnderflowCallback(SoundIoOutStream* stream)
{
    logWarn("Output underflow!\n");
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

int writeAudioOutputBuffer(int sourceBufferLength, float* sourceBufferPtr)
{
    lockMutex(audioOutMutex);
    int samplesToWrite = sourceBufferLength;
    int ringBufferFreeSpace = outBuffer->free();
    if(samplesToWrite > ringBufferFreeSpace)
    {
        samplesToWrite = ringBufferFreeSpace;
    }
    //logTerm("Write %d samples\n", samplesToWrite);

    outBuffer->write(samplesToWrite, sourceBufferPtr);
    outBuffer->advanceWritePointer(samplesToWrite);

    unlockMutex(audioOutMutex);

    return samplesToWrite;
}

int readAudioInputBuffer(int targetBufferLength, float* targetBufferPtr)
{
    lockMutex(audioInMutex);
    // NOTE: We don't need to take the number of channels into account here if we consider a "sample"
    //       to be a single sample from a single channel, but its important to note that that is what
    //       we're currently doing
    int samplesToWrite = targetBufferLength;
    int samplesAvailable = inBuffer->count();
    if(samplesAvailable < samplesToWrite)
        samplesToWrite = samplesAvailable;

    inBuffer->read(samplesToWrite, targetBufferPtr);
    inBuffer->advanceReadPointer(samplesToWrite);

    unlockMutex(audioInMutex);

    return samplesToWrite;
}

void enableMicrophone(bool enabled)
{
    if(enabled)
    {
        logInfo("Mic on\n");
    }
    else
    {
        logInfo("Mic off\n");
    }
    int error = soundio_instream_pause(inStream, !enabled);
    if(error)
    {
        logWarn("Error enabling microhpone\n");
    }
}

bool setAudioInputDevice(int newInputDevice)
{
    // TODO: We should probably wait for any currently running callbacks to finish
    if(inStream)
    {
        soundio_instream_destroy(inStream);
    }
    const SoundIoChannelLayout* monoLayout = soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);

    audioState.currentInputDevice = newInputDevice;
    inDevice = audioState.inputDeviceList[audioState.currentInputDevice];
    inStream = soundio_instream_create(inDevice);
    inStream->read_callback = inReadCallback;
    inStream->overflow_callback = inOverflowCallback;
    inStream->error_callback = inErrorCallback;
    inStream->sample_rate = soundio_device_nearest_sample_rate(inDevice, 48000);
    inStream->format = SoundIoFormatFloat32NE;
    inStream->layout = *monoLayout;
    inStream->software_latency = 0.005f;
    // NOTE: Lower latency corresponds to higher CPU usage, at 0.001 or 0s libsoundio eats an entire CPU but at 0.005 its fine
    // TODO: We probably want to check to make sure we don't set this to a value lower than the
    //       minimum latency for the current device

    int openError = soundio_instream_open(inStream);
    if(openError != SoundIoErrorNone)
    {
        logFail("Error opening input stream: %s\n", soundio_strerror(openError));
        soundio_instream_destroy(inStream);
        return false;
    }
    int startError = soundio_instream_start(inStream);
    if(startError != SoundIoErrorNone)
    {
        logFail("Error starting input stream: %s\n", soundio_strerror(startError));
        soundio_instream_destroy(inStream);
        return false;
    }

    const char* rawStr = inDevice->is_raw ? "(RAW) " : "";
    logInfo("Successfully opened audio input stream on device: %s %s\n", inDevice->name, rawStr);
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
    // TODO: Set all the other options

    int openError = soundio_outstream_open(outStream);
    if(openError != SoundIoErrorNone)
    {
        logFail("Error opening output stream: %s\n", soundio_strerror(openError));
        soundio_outstream_destroy(outStream);
        return false;
    }
    int startError = soundio_outstream_start(outStream);
    if(startError != SoundIoErrorNone)
    {
        logFail("Error starting output stream: %s\n", soundio_strerror(openError));
        soundio_outstream_destroy(outStream);
        return false;
    }

    const char* rawStr = outDevice->is_raw ? "(RAW) " : "";
    logInfo("Successfully opened audio output stream on device: %s %s\n", outDevice->name, rawStr);
    logInfo("  Sample Rate: %d\n", outStream->sample_rate);
    logInfo("  Latency: %0.8f\n", outStream->software_latency);
    logInfo("  Layout: %s\n", outStream->layout.name);
    logInfo("  Format: %s\n", soundio_format_string(outStream->format));
    return true;
}

void backendDisconnectCallback(SoundIo* sio, int error)
{
    logWarn("SoundIo backend disconnected: %s\n", soundio_strerror(error));
    // TODO: This runs immediately on flush_events on pIjIn's PC, it crashes if we do not
    //       specify this callback (which is probably correct, what is the appropriate response
    //       here even?)
}

void devicesChangeCallback(SoundIo* sio)
{
    // TODO: This doesn't appear to get called when I unplug/plug in my microphone (realtek does notice though)
    int inputDeviceCount = soundio_input_device_count(sio);
    int outputDeviceCount = soundio_output_device_count(sio);
    logInfo("SoundIo device list updated - %d input, %d output devices\n",
            inputDeviceCount, outputDeviceCount);

    if(audioState.currentInputDevice == -1)
    {
        // Try to set up a new input stream
        logInfo("Setup audio input\n");
        int defaultInputDevice = soundio_default_input_device_index(sio);
        int managedInputDeviceCount = 0;

        for(int i=0; i<inputDeviceCount; ++i)
        {
            SoundIoDevice* device = soundio_get_input_device(sio, i);
            bool isDefault = (i == defaultInputDevice);
            printDevice(device, isDefault);
            if(!device->is_raw)
            {
                managedInputDeviceCount += 1;
            }
            soundio_device_unref(device);
        }

        if(audioState.inputDeviceList)
            delete[] audioState.inputDeviceList;
        if(audioState.inputDeviceNames)
            delete[] audioState.inputDeviceNames;
        audioState.inputDeviceList = new SoundIoDevice*[managedInputDeviceCount];
        audioState.inputDeviceNames = new char*[managedInputDeviceCount];
        audioState.inputDeviceCount = managedInputDeviceCount;
        int defaultInputDeviceIndex = -1;
        int managedInputIndex = 0;
        for(int i=0; i<inputDeviceCount; ++i)
        {
            SoundIoDevice* device = soundio_get_input_device(sio, i);
            if(!device->is_raw)
            {
                audioState.inputDeviceList[managedInputIndex] = device;
                audioState.inputDeviceNames[managedInputIndex] = device->name;
                if(i == defaultInputDevice)
                {
                    defaultInputDeviceIndex = managedInputIndex;
                }
                managedInputIndex += 1;
            }
            else
            {
                soundio_device_unref(device);
            }
        }

        if(defaultInputDeviceIndex >= 0)
        {
            if(!setAudioInputDevice(defaultInputDeviceIndex))
            {
                logFail("Error: Unable to initialize audio input device %s\n",
                        audioState.inputDeviceNames[defaultInputDeviceIndex]);
            }
        }
    }

    if(audioState.currentOutputDevice == -1)
    {
        // Try to setup a new output stream
        logInfo("Setup audio output\n");
        int defaultOutputDevice = soundio_default_output_device_index(sio);
        int managedOutputDeviceCount = 0;
        for(int i=0; i<outputDeviceCount; ++i)
        {
            SoundIoDevice* device = soundio_get_output_device(sio, i);
            bool isDefault = (i == defaultOutputDevice);
            printDevice(device, isDefault);
            if(!device->is_raw)
            {
                managedOutputDeviceCount += 1;
            }
            soundio_device_unref(device);
        }

        if(audioState.outputDeviceList)
            delete[] audioState.outputDeviceList;
        if(audioState.outputDeviceNames)
            delete[] audioState.outputDeviceNames;
        audioState.outputDeviceList = new SoundIoDevice*[managedOutputDeviceCount];
        audioState.outputDeviceNames = new char*[managedOutputDeviceCount];
        audioState.outputDeviceCount = managedOutputDeviceCount;
        int defaultOutputDeviceIndex = -1;
        int managedOutputIndex = 0;
        for(int i=0; i<outputDeviceCount; ++i)
        {
            SoundIoDevice* device = soundio_get_output_device(sio, i);
            if(!device->is_raw)
            {
                audioState.outputDeviceList[managedOutputIndex] = device;
                audioState.outputDeviceNames[managedOutputIndex] = device->name;
                if(i == defaultOutputDevice)
                {
                    defaultOutputDeviceIndex = managedOutputIndex;
                }
                managedOutputIndex += 1;
            }
            else
            {
                soundio_device_unref(device);
            }
        }

        if(defaultOutputDeviceIndex >= 0)
        {
            if(!setAudioOutputDevice(defaultOutputDeviceIndex))
            {
                logFail("Error: Unable to initialize audio output device %s\n",
                        audioState.outputDeviceNames[defaultOutputDeviceIndex]);
            }
        }
    }
}

bool initAudio()
{
    // Initialize the current devices to null so that we will connect automatically when we 
    // get a list of connected devices
    audioState.currentInputDevice = -1;
    audioState.currentOutputDevice = -1;
    inBuffer = new RingBuffer(2400);
    audioInMutex = createMutex();
    outBuffer = new RingBuffer(48000);
    audioOutMutex = createMutex();

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

void deinitAudio()
{
    logInfo("Deinitialize audio subsystem\n");
    // TODO: Should we check that the mutexes are free at the moment? IE that any callbacks that
    //       may have been in progress when we stopped running, have finished

    soundio_instream_pause(inStream, true);
    soundio_instream_destroy(inStream);
    delete inBuffer;
    destroyMutex(audioInMutex);
    for(int i=0; i<audioState.inputDeviceCount; ++i)
    {
        soundio_device_unref(audioState.inputDeviceList[i]);
    }

    soundio_outstream_pause(outStream, true);
    soundio_outstream_destroy(outStream);
    delete outBuffer;
    destroyMutex(audioOutMutex);
    for(int i=0; i<audioState.outputDeviceCount; ++i)
    {
        soundio_device_unref(audioState.outputDeviceList[i]);
    }

    opus_encoder_destroy(encoder);
    opus_decoder_destroy(decoder);

    soundio_destroy(soundio);
}

// TODO: This is just here for testing, writing the audio recording to file
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
