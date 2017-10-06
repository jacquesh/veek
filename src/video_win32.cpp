#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG_VIDEO_IMAGE_OUTPUT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#ifdef DEBUG_VIDEO_VIDEO_OUTPUT
#include <time.h>
#include <stdlib.h> // For srand/rand
#endif

#include "common.h"
#include "logging.h"
#include "video.h"
#include "videoInput.h"

static int pixelBytes = 0;
static uint8* pixelValues = 0;
static uint8* preResizePixelValues = 0;

static videoInput VI;

bool Video::enableCamera(int deviceID)
{
    if(cameraDeviceCount == 0)
    {
        logWarn("Toggle camera failed: No available camera device\n");
        return false;
    }

    if(((deviceID < 0) || (deviceID != cameraDevice)) && (cameraDevice >= 0))
    {
        const char* deviceName = VI.getDeviceName(cameraDevice);
        logInfo("Disable camera: %s\n", deviceName);
        VI.stopDevice(cameraDevice);
        cameraDevice = -1;

        if(preResizePixelValues)
        {
            delete[] preResizePixelValues;
        }
    }

    if((deviceID >= 0) && (deviceID < cameraDeviceCount))
    {
        if(deviceID == cameraDevice)
        {
            return true;
        }

        cameraDevice = deviceID;
        const char* deviceName = VI.getDeviceName(cameraDevice);
        logInfo("Enable camera: %s\n", deviceName);

        bool success = VI.setupDevice(cameraDevice, cameraWidth, cameraHeight);
        if(success)
        {
            int actualWidth = VI.getWidth(cameraDevice);
            int actualHeight = VI.getHeight(cameraDevice);
            if((actualWidth != cameraWidth) || (actualHeight != cameraHeight))
            {
                preResizePixelValues = new uint8_t[actualWidth*actualHeight*3];
            }

            logInfo("Begin video capture using %s - Dimensions are %dx%d\n", deviceName,
                    actualWidth, actualHeight);
            return true;
        }
        else
        {
            logWarn("Failed to begin video capture using %s\n", deviceName);
        }
        return false;
    }

    return false;
}

bool Video::checkForNewVideoFrame()
{
    bool result = VI.isFrameNew(cameraDevice);
    if(result)
    {
        //logTerm("Recevied video input frame from local camera\n"); // TODO: Wraithy gets none of these
        if(preResizePixelValues)
        {
            int actualWidth = VI.getWidth(cameraDevice);
            int actualHeight = VI.getHeight(cameraDevice);

            VI.getPixels(cameraDevice, preResizePixelValues, true, true);

            stbir_resize_uint8(preResizePixelValues, actualWidth, actualHeight, 0,
                               pixelValues, cameraWidth, cameraHeight, 0, 3);
        }
        else
        {
            VI.getPixels(cameraDevice, pixelValues, true, true);
        }
    }
    return result;
}

uint8* Video::currentVideoFrame()
{
    return pixelValues;
}

bool SetupPlatform()
{
    pixelBytes = cameraWidth*cameraHeight*3;
    pixelValues = new uint8[pixelBytes];

    cameraDeviceCount = VI.listDevices();
    cameraDeviceNames = new char*[cameraDeviceCount];
    for(int i=0; i<cameraDeviceCount; i++)
    {
        const char* deviceName = VI.getDeviceName(i);
        cameraDeviceNames[i] = new char[strlen(deviceName)+1];
        strcpy(cameraDeviceNames[i], deviceName);
    }
    logInfo("%d video input devices available.\n", cameraDeviceCount);

    return true;
}

static void ShutdownPlatform()
{
    delete[] pixelValues;
    for(int i=0; i<cameraDeviceCount; i++)
    {
        delete[] cameraDeviceNames[i];
    }
    delete[] cameraDeviceNames;
}

