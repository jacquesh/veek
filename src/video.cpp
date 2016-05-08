#include <stdint.h>
#include <stdio.h>

#include "escapi.h"
#include "daala/codec.h"
#include "daala/daalaenc.h"
#include "daala/daaladec.h"

#include "common.h"

// https://people.xiph.org/~tterribe/pubs/lca2012/auckland/intro_to_video1.pdf

int deviceCount;

bool cameraEnabled = false;
int cameraDevice;
SimpleCapParams captureParams;


int cameraWidth = 320;
int cameraHeight = 240;
int pixelBytes = 0;
uint8* pixelValues = 0;

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

                int pixelIndex = (cameraHeight-y)*cameraWidth+ x;
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
    encoderInfo.nplanes = 3;
    encoderInfo.plane_info[0].xdec = 0;
    encoderInfo.plane_info[0].ydec = 0;
    encoderInfo.plane_info[1].xdec = 1;
    encoderInfo.plane_info[1].ydec = 1;
    encoderInfo.plane_info[2].xdec = 1;
    encoderInfo.plane_info[2].ydec = 1;
    encoderInfo.plane_info[3].xdec = 0;
    encoderInfo.plane_info[3].ydec = 0;
    encoderInfo.keyframe_rate = 256;

    daala_enc_ctx* encoderContext = daala_encode_create(&encoderInfo);
    daala_comment comment;
    daala_comment_init(&comment);
    daala_packet packet;

    while(daala_encode_flush_header(encoderContext, &comment, &packet) > 0)
    {
        //daala_comment_clear(&comment);
    }

    return true;
}

void deinitVideo()
{
    enableCamera(false);
    delete[] pixelValues;
    delete[] captureParams.mTargetBuf;
}
