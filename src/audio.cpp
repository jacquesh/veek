#include "audio.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "SDL_mutex.h"

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "vecmath.h"
#include "ringbuffer.h"

// TODO: Allow runtime switching of the recording/playback devices.
//       are things that libsoundio provides as a layer on top?
// TODO: The decoder states that we need to call decode with empty values for every lost packet
//       We do actually keep track of time but we have yet to use that to check for packet loss
//
// TODO: It might be worthwhile using our own ring buffer (we need mutexes anyways, better
//       information about how large of a block of data we can write at a time etc)
//       Also I dunno if libsoundio uses atomics outside of the ringbuffer, if not we can remove that
//       and then we can compile it ourselves? Seems weird, I dunno
AudioData audioState = {};

SoundIo* soundio;
OpusEncoder* encoder;
OpusDecoder* decoder;

SoundIoDevice* inDevice;
SoundIoInStream* inStream;
RingBuffer* inBuffer;
SDL_mutex* audioInMutex;

SoundIoDevice* outDevice;
SoundIoOutStream* outStream;
RingBuffer* outBuffer;
SDL_mutex* audioOutMutex;

void printDevice(SoundIoDevice* device)
{
    const char* rawStr = device->is_raw ? "(RAW)" : "";
    printf("%s%s\n", device->name, rawStr);
    if(device->probe_error != SoundIoErrorNone)
    {
        printf("Probe Error: %s\n", soundio_strerror(device->probe_error));
        return;
    }
    printf("  Channel count: %d\n", device->current_layout.channel_count);
    printf("  Sample Rate: %d\n", device->sample_rate_current);
    printf("  Latency: %0.8f - %0.8f\n", device->software_latency_min, device->software_latency_max);
    printf("      Now: %0.8f\n", device->software_latency_current);
}

void inReadCallback(SoundIoInStream* stream, int frameCountMin, int frameCountMax)
{
    // TODO: Wut? If we take (frameCountMin+frameCountMax)/2 then we seem to get called WAY too often
    //       and get random values, should probably check the error and overflow callbacks
    int framesRemaining = frameCountMax;//frameCountMin + (frameCountMax-frameCountMin)/2;
    //printf("Read callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    int channelCount = stream->layout.channel_count;
    SoundIoChannelArea* inArea;

    // TODO: Check the free space in inBuffer
    SDL_LockMutex(audioInMutex);
    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int readError = soundio_instream_begin_read(stream, &inArea, &frameCount);
        if(readError)
        {
            printf("Read error\n");
            break;
        }

        for(int frame=0; frame<frameCount; ++frame)
        {
            float val = *((float*)(inArea[0].ptr));
            inArea[0].ptr += inArea[0].step;

            inBuffer->write(val);
            inBuffer->advanceWritePointer(1);
            // TODO: Handle the layout (which we might force to be mono? see the initialization)
            /*
            for(int channel=0; channel<channelCount; ++channel)
            {
            }
            */
        }

        soundio_instream_end_read(stream);
        framesRemaining -= frameCount;
    }
    SDL_UnlockMutex(audioInMutex);
}

void outWriteCallback(SoundIoOutStream* stream, int frameCountMin, int frameCountMax)
{
    SDL_LockMutex(audioOutMutex);
    int framesAvailable = outBuffer->count();
    int framesRemaining = clamp(framesAvailable, frameCountMin, frameCountMax);
    //printf("Write callback! %d - %d => %d\n", frameCountMin, frameCountMax, framesRemaining);
    int channelCount = stream->layout.channel_count;
    SoundIoChannelArea* outArea;

    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int writeError = soundio_outstream_begin_write(stream, &outArea, &frameCount);
        if(writeError)
        {
            printf("Write error\n");
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
            float val = sourceBuffer->read();
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
    SDL_UnlockMutex(audioOutMutex);
}

void inOverflowCallback(SoundIoInStream* stream)
{
    printf("Input underflow!\n");
}

void outUnderflowCallback(SoundIoOutStream* stream)
{
    printf("Output underflow!\n");
}

void inErrorCallback(SoundIoInStream* stream, int error)
{
    printf("Input error: %s\n", soundio_strerror(error));
}

void outErrorCallback(SoundIoOutStream* stream, int error)
{
    printf("Output error: %s\n", soundio_strerror(error));
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

    while((sourceLengthRemaining > sizeof(int)) && (targetLengthRemaining > frameSize))
    {
        int packetLength = *((int*)sourceBuffer); // TODO: Byte order checking/swapping

        int correctErrors = 0; // TODO
        int framesDecoded = opus_decode_float(decoder,
                                              sourceBuffer+4, packetLength,
                                              targetBuffer, targetLengthRemaining,
                                              correctErrors);
        if(framesDecoded < 0)
        {
            printf("Error decoding audio data. Error %d\n", framesDecoded);
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
    while((sourceLengthRemaining > frameSize) && (targetLengthRemaining > frameSize))
    {
        // TODO: Multiple channels, this currently assumes a single channel
        int packetLength = opus_encode_float(encoder,
                                             sourceBuffer, frameSize,
                                             targetBuffer+4, targetLengthRemaining);
        *((int*)targetBuffer) = packetLength; // TODO: Byte order checking/swapping
        if(packetLength < 0)
        {
            printf("Error encoding audio. Error code %d\n", packetLength);
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
    SDL_LockMutex(audioOutMutex);
    // TODO: Consider the number of channels
    int samplesToWrite = sourceBufferLength;
    int ringBufferFreeSpace = outBuffer->free();
    if(samplesToWrite > ringBufferFreeSpace)
    {
        samplesToWrite = ringBufferFreeSpace;
    }
    //printf("Write %d samples\n", samplesToWrite);

    outBuffer->write(samplesToWrite, sourceBufferPtr);
    outBuffer->advanceWritePointer(samplesToWrite);

    SDL_UnlockMutex(audioOutMutex);

    return samplesToWrite;
}

int readAudioInputBuffer(int targetBufferLength, float* targetBufferPtr)
{
    SDL_LockMutex(audioInMutex);
    // NOTE: We don't need to take the number of channels into account here if we consider a "sample"
    //       to be a single sample from a single channel, but its important to note that that is what
    //       we're currently doing
    // TODO: Should we return the number of samples written including or excluding the channel count?
    int samplesToWrite = targetBufferLength;
    int samplesAvailable = inBuffer->count();
    if(samplesAvailable < samplesToWrite)
        samplesToWrite = samplesAvailable;
 
    inBuffer->read(samplesToWrite, targetBufferPtr);
    inBuffer->advanceReadPointer(samplesToWrite);

    SDL_UnlockMutex(audioInMutex);

    return samplesToWrite;
}

void enableMicrophone(bool enabled)
{
    if(enabled)
        printf("Mic on\n");
    else
        printf("Mic off\n");
    int error = soundio_instream_pause(inStream, !enabled);
    if(error)
    {
        printf("Error enabling microhpone\n");
    }
}

void setAudioInputDevice(int newInputDevice)
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
    inStream->sample_rate = 48000;
    inStream->format = SoundIoFormatFloat32NE;
    // TODO: Set all the other options, in particular can we force it to be mono?
    //       Surely we never get more than mono data from a single microphone by definition?

    int openError = soundio_instream_open(inStream);
    if(openError != SoundIoErrorNone)
    {
        printf("Error opening input stream: %s\n", soundio_strerror(openError));
    }
    int startError = soundio_instream_start(inStream);
    if(startError != SoundIoErrorNone)
    {
        printf("Error starting input stream: %s\n", soundio_strerror(startError));
    }
}

void setAudioOutputDevice(int newOutputDevice)
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
    outStream->sample_rate = 48000;
    outStream->format = SoundIoFormatFloat32NE;
    // TODO: Set all the other options

    int openError = soundio_outstream_open(outStream);
    if(openError != SoundIoErrorNone)
    {
        printf("Error opening output stream: %s\n", soundio_strerror(openError));
    }
    int startError = soundio_outstream_start(outStream);
    if(startError != SoundIoErrorNone)
    {
        printf("Error starting output stream: %s\n", soundio_strerror(openError));
    }
}

bool initAudio()
{
    RingBuffer* rbuf = new RingBuffer(10);
    for(int i=0; i<2; ++i)
    {
        rbuf->write((float)i);
        rbuf->advanceWritePointer(1);
    }
    float valsLeft[] = {3.14f, 4.21f, 5.12f};
    rbuf->write(3, valsLeft);
    rbuf->advanceWritePointer(3);

    for(int i=0; i<3; ++i)
    {
        printf("%.2f\n", rbuf->read());
        rbuf->advanceReadPointer(1);
    }

    rbuf->write(11.11f);
    rbuf->advanceWritePointer(1);
    float outVals[3];
    rbuf->read(3, outVals);
    rbuf->advanceReadPointer(3);


    // Initialize SoundIO
    soundio = soundio_create();
    if(!soundio)
    {
        return false;
    }
    int connectError = soundio_connect(soundio);
    if(connectError)
    {
        return false;
    }
    soundio_flush_events(soundio);
    // TODO: Check the supported input/output formats
    //       48000 Hz is probably VERY excessive

    printf("Initializing %s\n", opus_get_version_string());
    int opusError;
    opus_int32 opusSampleRate = 48000;
    int opusChannels = 1;
    int opusApplication = OPUS_APPLICATION_VOIP;
    encoder = opus_encoder_create(opusSampleRate, opusChannels, opusApplication, &opusError);
    printf("Opus Error from encoder creation: %d\n", opusError);
    decoder = opus_decoder_create(opusSampleRate, opusChannels, &opusError);
    printf("Opus Error from decoder creation: %d\n", opusError);

    // Setup input
    int defaultInputDevice = soundio_default_input_device_index(soundio);
    int inputDeviceCount = soundio_input_device_count(soundio);
    int managedInputDeviceCount = 0;
    for(int i=0; i<inputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_input_device(soundio, i);
        printDevice(device);
        if(!device->is_raw)
        {
            managedInputDeviceCount += 1;
        }
        soundio_device_unref(device);
    }
    audioState.defaultInputDevice = 0;
    audioState.inputDeviceCount = managedInputDeviceCount;
    audioState.inputDeviceList = new SoundIoDevice*[audioState.inputDeviceCount];
    audioState.inputDeviceNames = new char*[audioState.inputDeviceCount];
    int managedInputIndex = 0;
    for(int i=0; i<inputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_input_device(soundio, i);
        if(!device->is_raw)
        {
            audioState.inputDeviceList[managedInputIndex] = device;
            audioState.inputDeviceNames[managedInputIndex] = device->name;
            if(i == defaultInputDevice)
            {
                audioState.defaultInputDevice = managedInputIndex;
            }
            managedInputIndex += 1;
        }
        else
        {
            soundio_device_unref(device);
        }
    }
    inBuffer = new RingBuffer(48000);
    audioInMutex = SDL_CreateMutex();

    // Setup output
    int defaultOutputDevice = soundio_default_output_device_index(soundio);
    int outputDeviceCount = soundio_output_device_count(soundio);
    int managedOutputDeviceCount = 0;
    for(int i=0; i<outputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_output_device(soundio, i);
        if(!device->is_raw)
        {
            managedOutputDeviceCount += 1;
        }
        soundio_device_unref(device);
    }
    audioState.defaultOutputDevice = 0;
    audioState.outputDeviceCount = managedOutputDeviceCount;
    audioState.outputDeviceList = new SoundIoDevice*[audioState.outputDeviceCount];
    audioState.outputDeviceNames = new char*[audioState.outputDeviceCount];
    int managedOutputIndex = 0;
    for(int i=0; i<outputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_output_device(soundio, i);
        if(!device->is_raw)
        {
            audioState.outputDeviceList[managedOutputIndex] = device;
            audioState.outputDeviceNames[managedOutputIndex] = device->name;
            if(i == defaultOutputDevice)
            {
                audioState.defaultOutputDevice = managedOutputIndex;
            }
            managedOutputIndex += 1;
        }
        else
        {
            soundio_device_unref(device);
        }
    }
    outBuffer = new RingBuffer(48000);
    audioOutMutex = SDL_CreateMutex();

    setAudioInputDevice(audioState.defaultInputDevice);
    setAudioOutputDevice(audioState.defaultOutputDevice);
    return true;
}

void deinitAudio()
{
    // TODO: Should we check that the mutexes are free at the moment? IE that any callbacks that
    //       may have been in progress when we stopped running, have finished

    soundio_instream_pause(inStream, true);
    soundio_instream_destroy(inStream);
    delete inBuffer;
    SDL_DestroyMutex(audioInMutex);
    for(int i=0; i<audioState.inputDeviceCount; ++i)
    {
        soundio_device_unref(audioState.inputDeviceList[i]);
    }

    soundio_outstream_pause(outStream, true);
    soundio_outstream_destroy(outStream);
    delete outBuffer;
    SDL_DestroyMutex(audioOutMutex);
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
