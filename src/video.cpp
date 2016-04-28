#include <stdint.h>
#include <stdio.h>

#include "escapi.h"

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
    bool result = isCaptureDone(cameraDevice);
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
    deviceCount = setupESCAPI();
    printf("%d video input devices available.\n", deviceCount);
    if(deviceCount == 0)
    {
        return false;
    }

    pixelBytes = cameraWidth*cameraHeight*3;
    pixelValues = new uint8_t[pixelBytes];

    cameraDevice = deviceCount-1; // Can be anything in the range [0, deviceCount)

    captureParams.mWidth = cameraWidth;
    captureParams.mHeight = cameraHeight;
    captureParams.mTargetBuf = new int[cameraWidth*cameraHeight];

    enableCamera(false);
    return true;
}

void deinitVideo()
{
    enableCamera(false);
    delete[] pixelValues;
    delete[] captureParams.mTargetBuf;
}
