#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "escapi.h"

#include "ogg/ogg.h"
#include "theora/codec.h"
#include "theora/theora.h"
#include "theora/theoraenc.h"
#include "theora/theoradec.h"

#include "common.h"

// https://people.xiph.org/~tterribe/pubs/lca2012/auckland/intro_to_video1.pdf

static int deviceCount;

static bool cameraEnabled = false;
static int cameraDevice;
static SimpleCapParams captureParams;

int cameraWidth = 320; // TODO: We probably also want these to be static
int cameraHeight = 240;
static int pixelBytes = 0;
static uint8* pixelValues = 0;

static th_enc_ctx* encoderContext;
static th_dec_ctx* decoderContext;

void enableCamera(bool enabled)
{
    cameraEnabled = enabled;
    if(enabled)
    {
        char deviceName[256];
        getCaptureDeviceName(cameraDevice, deviceName, 256);
        log("Initializing %s\n", deviceName);

        initCapture(cameraDevice, &captureParams);
        doCapture(cameraDevice);
    }
    else
    {
        deinitCapture(cameraDevice);
    }
}

bool checkForNewVideoFrame()
{
    bool result = (isCaptureDone(cameraDevice) == 1);
    if(result)
    {
        for(int y=0; y<cameraHeight; ++y)
        {
            for(int x=0; x<cameraWidth; ++x)
            {
                int targetBufferIndex = y*cameraWidth+ x;
                int pixelVal = captureParams.mTargetBuf[targetBufferIndex];
                uint8* pixel = (uint8*)&pixelVal;
                uint8 red   = pixel[0];
                uint8 green = pixel[1];
                uint8 blue  = pixel[2];
                uint8 alpha = pixel[3];

                int pixelIndex = (cameraHeight-y-1)*cameraWidth+ x;
                pixelValues[3*pixelIndex + 0] = blue;
                pixelValues[3*pixelIndex + 1] = green;
                pixelValues[3*pixelIndex + 2] = red;
            }
        }
        doCapture(cameraDevice);
    }
    return result;
}

uint8* currentVideoFrame()
{
    return pixelValues;
}

void encodeRGBImage(uint8* inputData)
{
    th_ycbcr_buffer img;
    for(int i=0; i<3; i++)
    {
        img[i].width = 320;
        img[i].height = 240;
        img[i].stride = img[i].width;
        img[i].data = new uint8[img[i].width*img[i].height];
        for(int y=0; y<img[i].height; y++)
        {
            for(int x=0; x<img[i].width; x++)
            {
                int pixelIndex = y*img[i].width + x;
                img[i].data[pixelIndex] = inputData[pixelIndex];
            }
        }
    }

    ogg_packet packet;
    int result = th_encode_ycbcr_in(encoderContext, img);
    while(true){
        result = th_encode_packetout(encoderContext, 0, &packet);
        if(result <= 0)
            break;
    }
    log("%d Video Output bytes\n", packet.bytes);

    for(int i=0; i<3; i++)
    {
        delete[] img[i].data;
    }
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

    deviceCount = setupESCAPI();
    log("%d video input devices available.\n", deviceCount);
    if(deviceCount == 0)
    {
        return false;
    }
    cameraDevice = deviceCount-1; // Can be anything in the range [0, deviceCount)
    enableCamera(false);

    captureParams.mWidth = cameraWidth;
    captureParams.mHeight = cameraHeight;
    captureParams.mTargetBuf = new int[cameraWidth*cameraHeight];

    // Initialize theora encoder
    th_info encoderInfo;
    th_info_init(&encoderInfo);
    encoderInfo.pic_x = 0;
    encoderInfo.pic_y = 0;
    encoderInfo.pic_width = 320;
    encoderInfo.pic_height = 240;
    encoderInfo.frame_width = 320;
    encoderInfo.frame_height = 240;
    encoderInfo.pixel_fmt = TH_PF_444;
    encoderInfo.colorspace = TH_CS_UNSPECIFIED;
    encoderInfo.quality = 32;
    encoderInfo.fps_numerator = 20;
    encoderInfo.fps_denominator = 1;
    encoderInfo.aspect_numerator = 1;
    encoderInfo.aspect_denominator = 1;

    encoderContext = th_encode_alloc(&encoderInfo);
    th_comment comment;
    th_comment_init(&comment);
    ogg_packet tempPacket;
    ogg_packet headerPackets[3];
    int headerPacketIndex = 0;
    // NOTE: We need to copy each packet as we get to if we want to store it for later because
    //       the packet data is stored in a buffer that is owned by daala and gets re-used each
    //       time, meaning that if we DONT copy, the other headers will have their data overwritten
    while(th_encode_flushheader(encoderContext, &comment, &tempPacket) > 0)
    {
        th_comment_clear(&comment);
        copyTheoraPacket(headerPackets[headerPacketIndex], tempPacket);
        headerPacketIndex++;
        log("Output theora header packet\n");
    }
    assert(headerPacketIndex == 3);

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
        log("Headers left to decode: %d\n", headersRemaining);
        if(headersRemaining < 0)
            break;
    }
    th_comment_clear(&comment);
    decoderContext = th_decode_alloc(&decoderInfo, setupInfo);
    th_setup_free(setupInfo);

    // Encode example image
    th_ycbcr_buffer img;
    for(int i=0; i<3; i++)
    {
        img[i].width = 320;
        img[i].height = 240;
        img[i].stride = img[i].width;
        img[i].data = new uint8[img[i].width*img[i].height];
        for(int y=0; y<img[i].height; y++)
        {
            for(int x=0; x<img[i].width; x++)
            {
                int pixelIndex = y*img[i].width + x;
                img[i].data[pixelIndex] = 128;
            }
        }
    }

    int result;

    ogg_packet packet;
    result = th_encode_ycbcr_in(encoderContext, img);
    log("Image encoding result = %d\n", result);
    while(true){
        result = th_encode_packetout(encoderContext, 0, &packet);
        log("Video packet output result = %d\n", result);
        if(result <= 0)
            break;
    }
    log("%d Output bytes\n", packet.bytes);

    result = th_decode_packetin(decoderContext, &packet, 0);
    log("Video packet output result = %d\n", result);

    result = th_decode_ycbcr_out(decoderContext, img);
    log("Image decoding result = %d\n", result);

    return true;
}

void deinitVideo()
{
    enableCamera(false);
    delete[] pixelValues;
    delete[] captureParams.mTargetBuf;
}
