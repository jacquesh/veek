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

void printDevice(SoundIoDevice* device, bool isDefault)
{
    const char* defaultStr = isDefault ? "(DEFAULT)" : "";
    const char* rawStr = device->is_raw ? "(RAW)" : "";
    printf("%s%s%s\n", device->name, defaultStr, rawStr);
    if(device->probe_error)
    {
        printf("PROBE_ERROR!\n");
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

bool initAudio()
{
#if 0
    opus_encoder_get_size(2);
#endif

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
    int inputDeviceCount = soundio_input_device_count(soundio);
    int defaultInDevice = soundio_default_input_device_index(soundio);
    printf("%d SoundIO Input Devices:\n", inputDeviceCount);
    for(int i=0; i<inputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_input_device(soundio, i);
        if(!device->is_raw)
            printDevice(device, i == defaultInDevice);
        soundio_device_unref(device);
    }
    audioInMutex = SDL_CreateMutex();
    inBuffer = soundio_ring_buffer_create(soundio, 48000);

    inDevice = soundio_get_input_device(soundio, defaultInDevice);
    inStream = soundio_instream_create(inDevice);
    inStream->read_callback = inReadCallback;
    inStream->sample_rate = 48000;
    inStream->format = SoundIoFormatU8;
    // TODO: Set all the other options, in particular can we force it to be mono?
    //       Surely we never get more than mono data from a single microphone by definition?

    int micOpenError = soundio_instream_open(inStream);
    if(micOpenError)
    {
        printf("Error opening input stream\n");
    }
    int recorderror = soundio_instream_start(inStream);
    if(recorderror)
    {
        printf("error starting input stream\n");
    }

    // Setup output
    int outputDeviceCount = soundio_output_device_count(soundio);
    int defaultOutDevice = soundio_default_output_device_index(soundio);
    printf("%d SoundIO Output Devices:\n", outputDeviceCount);
    for(int i=0; i<outputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_output_device(soundio, i);
        if(!device->is_raw)
            printDevice(device, i== defaultOutDevice);
        soundio_device_unref(device);
    }
    audioOutMutex = SDL_CreateMutex();
    outBuffer = soundio_ring_buffer_create(soundio, 48000);

    outDevice = soundio_get_output_device(soundio, defaultOutDevice);
    outStream = soundio_outstream_create(outDevice);
    outStream->write_callback = outWriteCallback;
    outStream->sample_rate = 48000;
    outStream->format = SoundIoFormatU8;
    // TODO: Set all the other options

    int streamOpenError = soundio_outstream_open(outStream);
    if(streamOpenError)
    {
        printf("Error opening output stream\n");
    }
    int playError = soundio_outstream_start(outStream);
    if(playError)
    {
        printf("Error starting output stream\n");
    }

    return true;
}

void deinitAudio()
{
    soundio_instream_destroy(inStream);
    soundio_device_unref(inDevice);
    soundio_ring_buffer_destroy(inBuffer);
    SDL_DestroyMutex(audioInMutex);

    soundio_outstream_destroy(outStream);
    soundio_device_unref(outDevice);
    soundio_ring_buffer_destroy(outBuffer);
    SDL_DestroyMutex(audioOutMutex);

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
