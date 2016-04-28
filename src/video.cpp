#include <stdint.h>
#include <stdio.h>

#include "escapi.h"
#include "daala/codec.h"
#include "daala/daalaenc.h"
#include "daala/daaladec.h"

int deviceCount;

bool cameraEnabled = false;
int cameraDevice;
SimpleCapParams captureParams;


int cameraWidth = 320;
int cameraHeight = 240;
int pixelBytes = 0;
uint8_t* pixelValues = 0;

void enableCamera(bool enabled)
{
    cameraEnabled = enabled;
    if(enabled)
    {
        char deviceName[256];
        getCaptureDeviceName(cameraDevice, deviceName, 256);
        printf("Initializing %s\n", deviceName);

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
                uint8_t* pixel = (uint8_t*)&pixelVal;
                uint8_t red   = pixel[0];
                uint8_t green = pixel[1];
                uint8_t blue  = pixel[2];
                uint8_t alpha = pixel[3];

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

uint8_t* currentVideoFrame()
{
    return pixelValues;
}

bool initVideo()
{
    pixelBytes = cameraWidth*cameraHeight*3;
    pixelValues = new uint8_t[pixelBytes];

    deviceCount = setupESCAPI();
    printf("%d video input devices available.\n", deviceCount);
    if(deviceCount == 0)
    {
        return false;
    }
    cameraDevice = deviceCount-1; // Can be anything in the range [0, deviceCount)
    enableCamera(false);

    captureParams.mWidth = cameraWidth;
    captureParams.mHeight = cameraHeight;
    captureParams.mTargetBuf = new int[cameraWidth*cameraHeight];

#if 0
    daala_log_init();
    daala_info encoderInfo;
    daala_info_init(&encoderInfo);
    encoderInfo.pic_width = 320;
    encoderInfo.pic_height = 240;
    encoderInfo.bitdepth_mode = OD_BITDEPTH_MODE_8;
    encoderInfo.timebase_numerator = ;
    encoderInfo.timebase_denominator = ;
    encoderInfo.frame_duration = ;
    encoderInfo.aspect_numerator = ;
    encoderInfo.aspect_denominator = ;
    encoderInfo.full_precision_references = ;
    encoderInfo.nplanes = ;
    encoderInfo.keyframe_rate = ;

    daala_enc_ctx* encoderContext = daala_encode_create(&encoderInfo);
#endif

    return true;
}

void deinitVideo()
{
    enableCamera(false);
    delete[] pixelValues;
    delete[] captureParams.mTargetBuf;
}
