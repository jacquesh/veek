#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "theora/theoraenc.h"
#include "theora/theoradec.h"

#include "videoinput.h"

#ifdef DEBUG_VIDEO_IMAGE_OUTPUT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#ifdef DEBUG_VIDEO_VIDEO_OUTPUT
#include <time.h>
#include <stdlib.h> // For srand/rand
#endif

#include "common.h"
#include "logging.h"

// https://people.xiph.org/~tterribe/pubs/lca2012/auckland/intro_to_video1.pdf

static int deviceCount;

static bool cameraEnabled = false;
static int cameraDevice;

static videoInput VI;

int cameraWidth = 320; // TODO: We probably also want these to be static
int cameraHeight = 240;
static int pixelBytes = 0;
static uint8* pixelValues = 0;

static th_enc_ctx* encoderContext;
static th_dec_ctx* decoderContext;

static th_ycbcr_buffer encodingImage;
static th_ycbcr_buffer decodingImage;

#ifdef DEBUG_VIDEO_VIDEO_OUTPUT
static FILE* ogvOutputFile;
static ogg_stream_state ogvOutputStream;
#endif

#include <windows.h>

bool enableCamera(bool enabled)
{
    const char* toggleString = enabled ? "Enable" : "Disable";
    if(cameraDevice < 0)
    {
        logWarn("%s camera failed: No active camera device\n", toggleString);
        return cameraEnabled;
    }

    if(enabled)
    {
        if(deviceCount == 0)
            return false;

        cameraDevice = 0;
        const char* deviceName = VI.getDeviceName(cameraDevice);
        logInfo("%s camera: %s\n", toggleString, deviceName);

        bool success = VI.setupDevice(cameraDevice, cameraWidth, cameraHeight);
        if(success)
        {
            logInfo("Begin video capture using %s - Dimensions are %dx%dx%d\n", deviceName,
                    VI.getWidth(cameraDevice), VI.getHeight(cameraDevice), VI.getSize(cameraDevice));
        }
        else
        {
            logWarn("Failed to begin video capture using %s\n", deviceName);
        }
    }
    else
    {
        const char* deviceName = VI.getDeviceName(cameraDevice);
        logInfo("%s camera: %s\n", toggleString, deviceName);

        VI.stopDevice(cameraDevice);
    }

    cameraEnabled = enabled;
    return enabled;
}

bool checkForNewVideoFrame()
{
    bool result = VI.isFrameNew(cameraDevice);
    if(result)
    {
        //logTerm("Recevied video input frame from local camera\n"); // TODO: Wraithy gets none of these
        VI.getPixels(cameraDevice, pixelValues, true);
    }
    return result;
}

uint8* currentVideoFrame()
{
    return pixelValues;
}

int encodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer)
{
    // TODO: Support other/variable image sizes
    assert(inputLength == 320*240*3);

#ifdef DEBUG_VIDEO_IMAGE_OUTPUT
    char outpngName[64];
    static int pngIndex = 0;
    if(pngIndex <= 4096)
    {
        pngIndex++;
        sprintf(outpngName, "webcam_capture_%04d.png", pngIndex);
        int success = stbi_write_png(outpngName, 320, 240, 3, pixelValues, 320*3);
    }
#endif

    // Convert RGB image to Y'CbCr
    for(int y=0; y<encodingImage[0].height; y++)
    {
        for(int x=0; x<encodingImage[0].width; x++)
        {
            // TODO: We're assuming here that each image plane has the same size
            int pixelIndex = y*encodingImage[0].width + x;
            uint8 r = inputBuffer[3*pixelIndex + 0];
            uint8 g = inputBuffer[3*pixelIndex + 1];
            uint8 b = inputBuffer[3*pixelIndex + 2];

            // TODO: Read http://www.equasys.de/colorconversion.html
#if 0
            uint8 YPrime = (uint8)(0.299f*r + 0.587f*g + 0.114f*b);
            uint8 Cb = (uint8)((0.436f*255.0f - 0.14713f*r - 0.28886f*g + 0.436f*b)/0.872f);
            uint8 Cr = (uint8)((0.615f*255.0f + 0.615f*r - 0.51499f*g - 0.10001f*b)/1.230f);
#else
            uint8 YPrime = (uint8)(16 + 0.257f*r + 0.504f*g + 0.098f*b);
            uint8 Cb = (uint8)(128 - 0.148f*r - 0.291f*g + 0.439f*b);
            uint8 Cr = (uint8)(128 + 0.439f*r - 0.368f*g - 0.071f*b);
#endif
            *(encodingImage[0].data + y*encodingImage[0].stride + x) = YPrime;
            *(encodingImage[1].data + y*encodingImage[1].stride + x) = Cb;
            *(encodingImage[2].data + y*encodingImage[2].stride + x) = Cr;
        }
    }

    // Encode image
    int bytesWritten = 0;
    uint8* bufferPtr = outputBuffer;

    ogg_packet packet;
    int result = th_encode_ycbcr_in(encoderContext, encodingImage);
    if(result < 0)
    {
        logWarn("ERROR: Image encoding failed with code %d\n", result);
        return 0;
    }

    int packetsExtracted = 0;
    while(true)
    {
        result = th_encode_packetout(encoderContext, 0, &packet);
        if(result <= 0)
            break;

        packetsExtracted += 1;
        // Write to output buffer
        // TODO: For now we're just assuming that the buffer has enough space, in future
        //       we probably want to handle it by just storing the packet and checking next time
        //       before we actually do any encoding, filling in any old frames
        assert(outputLength - bytesWritten >= sizeof(int32)+packet.bytes);
        *((int32*)bufferPtr) = packet.bytes;
        memcpy(bufferPtr+sizeof(int32), packet.packet, packet.bytes);

        bytesWritten += sizeof(int32) + packet.bytes;
        bufferPtr += sizeof(int32) + packet.bytes;

#ifdef DEBUG_VIDEO_VIDEO_OUTPUT
        // TODO: Write encoded images out to an ogv file
        ogg_page page;
        ogg_stream_packetin(&ogvOutputStream, &packet);
        while(ogg_stream_pageout(&ogvOutputStream, &page))
        {
            fwrite(page.header, page.header_len, 1, ogvOutputFile);
            fwrite(page.body, page.body_len, 1, ogvOutputFile);
        }
#endif
    }

    return bytesWritten;
}

static float clamp(float x)
{
    if(x < 0.0f)
        return 0.0f;
    else if(x > 255.0f)
        return 255.0f;
    return x;
}

int decodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer)
{
    // TODO: Support other image sizes
    assert(outputLength == 320*240*3);

    int inBytesRemaining = inputLength;
    ogg_packet packet;

    while(inBytesRemaining > 0)
    {
        // NOTE: decode_packetin does not appear to use any members of packet other than
        //       packet.bytes and packet.packet
        packet.bytes = *((int32*)inputBuffer);
        packet.packet = inputBuffer + sizeof(int32);

        int result = th_decode_packetin(decoderContext, &packet, 0);
        if(result < 0)
        {
            logWarn("ERROR: Video packet decode failed with code: %d\n", result);
            return 0;
        }

        // TODO: Can we ever get more than one image out here?
        result = th_decode_ycbcr_out(decoderContext, decodingImage);
        if(result < 0)
        {
            logWarn("ERROR: Video frame extraction failed with code: %d\n", result);
            return 0;
        }

        int imageWidth = decodingImage[0].width;
        int imageHeight = decodingImage[0].height;
        for(int y=0; y<imageHeight; y++)
        {
            for(int x=0; x<imageWidth; x++)
            {
                int pixelIndex = y*imageWidth+ x;
                uint8 Y = *(decodingImage[0].data + y*decodingImage[0].stride + x);
                uint8 Cb= *(decodingImage[1].data + y*decodingImage[1].stride + x);
                uint8 Cr= *(decodingImage[2].data + y*decodingImage[2].stride + x);
                float YNew = (float)Y - 16.0f;
                float CbNew= (float)Cb - 128.0f;
                float CrNew= (float)Cr - 128.0f;
                uint8 r = (uint8)clamp(1.164f*YNew + 0.000f*CbNew + 1.596f*CrNew);
                uint8 g = (uint8)clamp(1.164f*YNew - 0.392f*CbNew - 0.813f*CrNew);
                uint8 b = (uint8)clamp(1.164f*YNew + 2.017f*CbNew + 0.000f*CrNew);

                outputBuffer[3*pixelIndex + 0] = r;
                outputBuffer[3*pixelIndex + 1] = g;
                outputBuffer[3*pixelIndex + 2] = b;
            }
        }
        inBytesRemaining -= imageWidth*imageHeight*3;
    }
    return 0;
}

static void copyTheoraPacket(ogg_packet& out, ogg_packet& in)
{
    out.packet = new uint8[in.bytes];
    memcpy(out.packet, in.packet, in.bytes);
    out.bytes = in.bytes;
    out.b_o_s = in.b_o_s;
    out.e_o_s = in.e_o_s;
    out.granulepos = in.granulepos;
    out.packetno = in.packetno;
}

bool initVideo()
{
    pixelBytes = cameraWidth*cameraHeight*3;
    pixelValues = new uint8[pixelBytes];

    deviceCount = VI.listDevices();
    logInfo("%d video input devices available.\n", deviceCount);

    // Initialize theora encoder
    th_info encoderInfo;
    th_info_init(&encoderInfo);
    encoderInfo.pic_x = 0;
    encoderInfo.pic_y = 0;
    encoderInfo.pic_width = 320;
    encoderInfo.pic_height = 240;
    encoderInfo.frame_width = 320; // Must be a multiple of 16
    encoderInfo.frame_height = 240;// Must be a multiple of 16
    encoderInfo.pixel_fmt = TH_PF_444;
    encoderInfo.colorspace = TH_CS_UNSPECIFIED;
    encoderInfo.quality = 48;
    encoderInfo.target_bitrate=  -1;
    encoderInfo.fps_numerator = 24;
    encoderInfo.fps_denominator = 1;
    encoderInfo.aspect_numerator = 0;
    encoderInfo.aspect_denominator = 0;
    //encoderInfo.keyframe_granule_shift=...; // TODO

    encoderContext = th_encode_alloc(&encoderInfo);
    th_info_clear(&encoderInfo);

    th_comment comment;
    th_comment_init(&comment);
    ogg_packet tempPacket;
    ogg_packet headerPackets[3];
    int headerPacketCount = 0;
    // NOTE: We need to copy each packet as we get to if we want to store it for later because
    //       the packet data is stored in a buffer that is owned by daala and gets re-used each
    //       time, meaning that if we DONT copy, the other headers will have their data overwritten
    while(th_encode_flushheader(encoderContext, &comment, &tempPacket) > 0)
    {
        th_comment_clear(&comment);
        copyTheoraPacket(headerPackets[headerPacketCount], tempPacket);
        headerPacketCount++;
        logInfo("Output theora header packet\n");
    }
    assert(headerPacketCount == 3);

    // TODO: Support other image sizes
    for(int i=0; i<3; i++)
    {
        encodingImage[i].width = 320;
        encodingImage[i].height = 240;
        encodingImage[i].stride = encodingImage[i].width;
        encodingImage[i].data = new uint8[encodingImage[i].width*encodingImage[i].height];
    }

    // Initialize theora decoder
    th_info decoderInfo;
    th_info_init(&decoderInfo);
    //th_comment comment;
    th_comment_init(&comment);
    th_setup_info* setupInfo = NULL;
    for(int i=0; i<3; i++)
    {
        int headersRemaining = th_decode_headerin(&decoderInfo, &comment,
                                                  &setupInfo, &headerPackets[i]);
        logInfo("Headers left to decode: %d\n", headersRemaining);
        if(headersRemaining < 0)
            break;
    }
    th_comment_clear(&comment);
    decoderContext = th_decode_alloc(&decoderInfo, setupInfo);
    th_setup_free(setupInfo);

#ifdef DEBUG_VIDEO_VIDEO_OUTPUT
    ogvOutputFile = fopen("debug_videoinput.ogv", "wb");
    if(!ogvOutputFile)
    {
        logWarn("Error: Unable to open ogg video output file\n");
        return false;
    }

    srand(time(NULL));
    if(ogg_stream_init(&ogvOutputStream, rand()))
    {
        logWarn("Error: Unable to create ogg video output stream\n");
        return false;
    }

    // TODO: The example calls this separately for the first packet, does it get its own page?
    ogg_page headerPage;
    for(int headerIndex=0; headerIndex<headerPacketCount; headerIndex++)
    {
        ogg_stream_packetin(&ogvOutputStream, &headerPackets[headerIndex]);
        if(ogg_stream_pageout(&ogvOutputStream, &headerPage))
        {
            fwrite(headerPage.header, headerPage.header_len, 1, ogvOutputFile);
            fwrite(headerPage.body, headerPage.body_len, 1, ogvOutputFile);
        }
    }

    // NOTE: We flush any remaining header data here so that the first set of actual data
    //       starts on a new page, as per the ogg spec
    while(ogg_stream_flush(&ogvOutputStream, &headerPage))
    {
        fwrite(headerPage.header, headerPage.header_len, 1, ogvOutputFile);
        fwrite(headerPage.body, headerPage.body_len, 1, ogvOutputFile);
    }
#endif

    return true;
}

void deinitVideo()
{
    logInfo("Deinitialize video subsystem\n");
    enableCamera(false);
    delete[] pixelValues;

    for(int i=0; i<3; i++)
    {
        delete[] encodingImage[i].data;
    }
    th_encode_free(encoderContext);
    th_decode_free(decoderContext);

#ifdef DEBUG_VIDEO_VIDEO_OUTPUT
    ogg_page outputPage;
    if(ogg_stream_flush(&ogvOutputStream, &outputPage))
    {
        fwrite(outputPage.header, outputPage.header_len, 1, ogvOutputFile);
        fwrite(outputPage.body, outputPage.body_len, 1, ogvOutputFile);
    }
    ogg_stream_clear(&ogvOutputStream);

    if(ogvOutputFile)
    {
        fflush(ogvOutputFile);
        fclose(ogvOutputFile);
    }
#endif
}
