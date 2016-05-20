#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "escapi.h"
#include "daala/codec.h"
#include "daala/daalaenc.h"
#include "daala/daaladec.h"

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

static void copyDaalaPacket(daala_packet& out, daala_packet& in)
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

    daala_log_init();

    // Init daala encoder
    daala_info encoderInfo;
    daala_info_init(&encoderInfo);
    encoderInfo.pic_width = 320;
    encoderInfo.pic_height = 240;
    //encoderInfo.bitdepth_mode = OD_BITDEPTH_MODE_8; // NOTE: This is set by info_init
    encoderInfo.timebase_numerator = 20; // 20fps
    encoderInfo.timebase_denominator = 0;
    encoderInfo.frame_duration = 1;
    encoderInfo.pixel_aspect_numerator = 4;
    encoderInfo.pixel_aspect_denominator = 3;
    encoderInfo.full_precision_references = 0;
    encoderInfo.nplanes = 1;
    encoderInfo.plane_info[0].xdec = 0;
    encoderInfo.plane_info[0].ydec = 0;
    encoderInfo.keyframe_rate = 256;

    daala_enc_ctx* encoderContext = daala_encode_create(&encoderInfo);
    daala_comment comment;
    daala_comment_init(&comment);
    daala_packet headerPackets[3];
    daala_packet tempPacket;
    // NOTE: We need to copy each packet as we get to if we want to store it for later because
    //       the packet data is stored in a buffer that is owned by daala and gets re-used each
    //       time, meaning that if we DONT copy, the other headers will have their data overwritten
    if(daala_encode_flush_header(encoderContext, &comment, &tempPacket) <= 0)
    {
        log("Interal Daala error\n");
    }
    copyDaalaPacket(headerPackets[0], tempPacket);

    int headerPacketIndex = 1;
    while(daala_encode_flush_header(encoderContext, &comment, &tempPacket) > 0)
    {
        copyDaalaPacket(headerPackets[headerPacketIndex], tempPacket);
        headerPacketIndex++;
    }
    log("%d header packets created\n", headerPacketIndex);

    // Init daala decoder
    daala_info decoderInfo;
    daala_info_init(&decoderInfo);
    //daala_comment comment;
    daala_comment_init(&comment);
    daala_setup_info* setupInfo = NULL;
    for(int i=0; i<3; i++)
    {
        int headersRemaining = daala_decode_header_in(&decoderInfo, &comment,
                                                      &setupInfo, &(headerPackets[i]));
        log("Headers left to decode: %d\n", headersRemaining);
        if(headersRemaining < 0)
            break;
    }
    daala_comment_clear(&comment);
    daala_dec_ctx* decoderContext = daala_decode_create(&decoderInfo, setupInfo);
    daala_setup_free(setupInfo);

    // Encode example frame
    daala_image img;
    img.width = 320;
    img.height = 240;
    img.nplanes = 1;
    img.planes[0].xdec = 0;
    img.planes[0].ydec = 0;
    img.planes[0].bitdepth = 8;
    img.planes[0].xstride = 1;
    img.planes[0].ystride = 320;
    img.planes[0].data = new uint8[img.width*img.height];
    for(int y=0; y<img.height; y++)
    {
        for(int x=0; x<img.width; x++)
        {
            size_t pixelIndex = y*img.width + x;
            img.planes[0].data[pixelIndex] = 128;
        }
    }

    int result = daala_encode_img_in(encoderContext, &img, 0);
    log("Image Encoding Result = %d\n", result);

    daala_packet packet; 
    while(true){
        result = daala_encode_packet_out(encoderContext, 0, &packet);
        log("Video Packet Output Result = %d\n", result);
        if(result <= 0)
            break;
    }

    // Decode Example Frame
    result = daala_decode_packet_in(decoderContext, &packet);
    log("Video Packet Input Result = %d\n", result);

    while(true)
    {
        result = daala_decode_img_out(decoderContext, &img);
        log("Image Decoding Result = %d\n", result);
        if(result <= 0)
            break;
    }

    return true;
}

void deinitVideo()
{
    enableCamera(false);
    delete[] pixelValues;
    delete[] captureParams.mTargetBuf;
}
