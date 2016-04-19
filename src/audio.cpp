#include "audio.h"
#include <stdio.h>

#include <stdint.h>

#include "soundio/soundio.h"

SoundIo* soundio;
SoundIoDevice* inDevice;
SoundIoDevice* outDevice;
SoundIoInStream* inStream;
SoundIoOutStream* outStream;

SoundIoRingBuffer* ringBuffer;

uint8_t audioInBuffer[96000];
int bufferLoc = 0;

FILE* audioFile;
int audioSize = 0;
WAVEHEADER audioHeader = {};

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

void inReadCallback(SoundIoInStream* inStream, int frameCountMin, int frameCountMax)
{
    int framesRemaining = frameCountMax;
    int channelCount = outStream->layout.channel_count;
    SoundIoChannelArea* inArea;

    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int readError = soundio_instream_begin_read(inStream, &inArea, &frameCount);
        if(readError)
        {
            printf("Read error\n");
            break;
        }

        for(int frame=0; frame<frameCount; ++frame)
        {
            uint8_t val = *((uint8_t*)(inArea[0].ptr));
            inArea[0].ptr += inArea[0].step;

            char* ringPtr = soundio_ring_buffer_write_ptr(ringBuffer);
            soundio_ring_buffer_advance_write_ptr(ringBuffer, 1);
            *ringPtr = val;

            if(bufferLoc < 96000)
            {
                audioInBuffer[bufferLoc] = val;
                bufferLoc += 1;
            }
            /*
            for(int channel=0; channel<channelCount; ++channel)
            {
            }
            */
        }

        soundio_instream_end_read(inStream);
        framesRemaining -= frameCount;
    }
}

void outWriteCallback(SoundIoOutStream* outStream, int frameCountMin, int frameCountMax)
{
    printf("Write callback! %d - %d\n", frameCountMin, frameCountMax);
    int framesRemaining = frameCountMax;
    int channelCount = outStream->layout.channel_count;
    SoundIoChannelArea* outArea;

    while(framesRemaining > 0)
    {
        int frameCount = framesRemaining;
        int writeError = soundio_outstream_begin_write(outStream, &outArea, &frameCount);
        if(writeError)
        {
            printf("Write error\n");
            break;
        }

        for(int frame=0; frame<frameCount; ++frame)
        {
            uint8_t val = 128;//(uint8_t)(rand() % 255);
            for(int channel=0; channel<channelCount; ++channel)
            {
                *(outArea[channel].ptr) = val;
                outArea[channel].ptr += outArea[channel].step;
            }
        }

        soundio_outstream_end_write(outStream);
        framesRemaining -= frameCount;
    }
}

bool initAudio()
{
    soundio = soundio_create();
    if(!soundio)
    {
        return false;
    }
    ringBuffer = soundio_ring_buffer_create(soundio, 4800);
    int connectError = soundio_connect(soundio);
    if(connectError)
    {
        return false;
    }
    soundio_flush_events(soundio);

    int inputDeviceCount = soundio_input_device_count(soundio);
    int outputDeviceCount = soundio_output_device_count(soundio);
    int defaultInDevice = soundio_default_input_device_index(soundio);
    int defaultOutDevice = soundio_default_output_device_index(soundio);

    printf("%d SoundIO Input Devices:\n", inputDeviceCount);
    for(int i=0; i<inputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_input_device(soundio, i);
        if(!device->is_raw)
            printDevice(device, i == defaultInDevice);
        soundio_device_unref(device);
    }
    printf("%d SoundIO Output Devices:\n", outputDeviceCount);
    for(int i=0; i<outputDeviceCount; ++i)
    {
        SoundIoDevice* device = soundio_get_output_device(soundio, i);
        if(!device->is_raw)
            printDevice(device, i== defaultOutDevice);
        soundio_device_unref(device);
    }

    inDevice = soundio_get_input_device(soundio, defaultInDevice);
    inStream = soundio_instream_create(inDevice);
    inStream->read_callback = inReadCallback;
    inStream->sample_rate = 48000;
    inStream->format = SoundIoFormatU8;
    int micOpenError = soundio_instream_open(inStream);
    if(micOpenError)
    {
        printf("Error opening stream\n");
    }

    int recordError = soundio_instream_start(inStream);
    if(recordError)
    {
        printf("Error starting recording\n");
    }

    outDevice = soundio_get_output_device(soundio, defaultOutDevice);
    outStream = soundio_outstream_create(outDevice);
    outStream->write_callback = outWriteCallback;
    outStream->sample_rate = 48000;
    outStream->format = SoundIoFormatU8;
    // TODO: There are more options that you can use here

    int streamOpenError = soundio_outstream_open(outStream);
    if(streamOpenError)
    {
        printf("Error opening stream\n");
    }

    int playError = soundio_outstream_start(outStream);
    if(playError)
    {
        printf("Error starting stream\n");
    }


    // TODO: This is debug code for writing audio to file
    audioFile = fopen("out.wav", "wb");
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

    return true;
}

void deinitAudio()
{
    soundio_outstream_destroy(outStream);
    soundio_device_unref(outDevice);
    soundio_destroy(soundio);

    int bufferBytes = 96000;
    if(bufferLoc < bufferBytes)
    {
        bufferBytes = bufferLoc;
    }
    fwrite(audioInBuffer, bufferBytes, 1, audioFile);
    fseek(audioFile, 4, SEEK_SET);
    int size = audioSize + sizeof(WAVEHEADER) - 8;
    fwrite(&size, 4, 1, audioFile);
    fseek(audioFile, 42, SEEK_SET);
    fwrite(&audioSize, 4, 1, audioFile);
    fclose(audioFile);
}
