#include "audio.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "SDL_mutex.h"

#include "soundio/soundio.h"
#include "opus/opus.h"

#include "vecmath.h"

SoundIo* soundio;

SoundIoDevice* inDevice;
SoundIoInStream* inStream;
SoundIoRingBuffer* inBuffer;
SDL_mutex* audioInMutex;

SoundIoDevice* outDevice;
SoundIoOutStream* outStream;
SoundIoRingBuffer* outBuffer;
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
    int framesRemaining = frameCountMin + (frameCountMax-frameCountMin)/2;
    int channelCount = stream->layout.channel_count;
    SoundIoChannelArea* inArea;

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
            uint8_t val = *((uint8_t*)(inArea[0].ptr));
            inArea[0].ptr += inArea[0].step;

            uint8_t* writePtr = (uint8_t*)soundio_ring_buffer_write_ptr(inBuffer);
            *writePtr = val;
            soundio_ring_buffer_advance_write_ptr(inBuffer, 1);
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
#if 0
    printf("Write callback! %d - %d\n", frameCountMin, frameCountMax);
#endif
    SDL_LockMutex(audioOutMutex);
    int framesAvailable = soundio_ring_buffer_fill_count(outBuffer);
    int framesRemaining = clamp(framesAvailable, frameCountMin, frameCountMax);
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

        framesAvailable = soundio_ring_buffer_fill_count(outBuffer);
        if(framesAvailable > frameCount)
        {
            framesAvailable = frameCount; // NOTE: We need to ensure that we don't write too far
        }
        for(int frame=0; frame<framesAvailable; ++frame)
        {
            uint8_t* readPtr = (uint8_t*)soundio_ring_buffer_read_ptr(outBuffer);
            uint8_t val = *readPtr;

            for(int channel=0; channel<channelCount; ++channel)
            {
                uint8_t* samplePtr = (uint8_t*)outArea[channel].ptr;
                *samplePtr = val;
                outArea[channel].ptr += outArea[channel].step;
            }
            soundio_ring_buffer_advance_read_ptr(outBuffer, 1);
        }

        for(int frame=framesAvailable; frame<frameCount; ++frame)
        {
            uint8_t silence = 128;
            for(int channel=0; channel<channelCount; ++channel)
            {
                uint8_t* samplePtr = (uint8_t*)outArea[channel].ptr;
                *samplePtr = silence;
                outArea[channel].ptr += outArea[channel].step;
            }
        }

        soundio_outstream_end_write(stream);
        framesRemaining -= frameCount;

#if 0
        printf("Wrote %d frames from the buffer and %d frames of silence\n", framesAvailable, frameCount-framesAvailable);
#endif
    }
    SDL_UnlockMutex(audioOutMutex);
}

void readToAudioOutputBuffer(uint32_t timestamp, uint32_t length, uint8_t* data)
{
    SDL_LockMutex(audioOutMutex);
    for(uint32_t i=0; i<length; ++i)
    {
        char* writePtr = soundio_ring_buffer_write_ptr(outBuffer);
        soundio_ring_buffer_advance_write_ptr(outBuffer, 1);

        uint8_t* buf = (uint8_t*)writePtr;
        *buf = data[i];
    }
    SDL_UnlockMutex(audioOutMutex);
}

void writeFromAudioInputBuffer(uint32_t bufferSize, uint8_t* buffer)
{
    SDL_LockMutex(audioInMutex);
    int inBufferSamplesAvailable = soundio_ring_buffer_fill_count(inBuffer);
    int samplesFromBuffer = bufferSize;
    if(inBufferSamplesAvailable < samplesFromBuffer)
        samplesFromBuffer = inBufferSamplesAvailable;

    // Fill the buffer as far as possible with samples from the microphone
    for(int i=0; i<samplesFromBuffer; ++i)
    {
        char* readPtr = soundio_ring_buffer_read_ptr(inBuffer);
        soundio_ring_buffer_advance_read_ptr(inBuffer, 1);

        uint8_t* buf = (uint8_t*)readPtr;
        buffer[i] = *buf;
    }

    // Fill any remaining buffer space with silence
    for(uint32_t i=samplesFromBuffer; i<bufferSize; ++i)
    {
        buffer[i] = 128;
    }
    SDL_UnlockMutex(audioInMutex);
}

void enableMicrophone(bool enabled)
{
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
    inBuffer = soundio_ring_buffer_create(soundio, 48000*sizeof(float));
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
    outBuffer = soundio_ring_buffer_create(soundio, 48000*sizeof(float));
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
    soundio_ring_buffer_destroy(inBuffer);
    SDL_DestroyMutex(audioInMutex);
    for(int i=0; i<audioState.inputDeviceCount; ++i)
    {
        soundio_device_unref(audioState.inputDeviceList[i]);
    }

    soundio_outstream_pause(outStream, true);
    soundio_outstream_destroy(outStream);
    soundio_ring_buffer_destroy(outBuffer);
    SDL_DestroyMutex(audioOutMutex);
    for(int i=0; i<audioState.outputDeviceCount; ++i)
    {
        soundio_device_unref(audioState.outputDeviceList[i]);
    }

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
