#include "videoinput.h"

static videoInput VI;

void cc_initialize()
{
    VI.listDevices();
}

int cc_deviceCount()
{
    return VI.listDevices(true);
}

const char* cc_deviceName(int deviceID)
{
    return VI.getDeviceName(deviceID);
}

bool cc_enableDevice(int deviceID, int width, int height)
{
    return VI.setupDevice(deviceID, width, height);
}

void cc_disableDevice(int deviceID)
{
    VI.stopDevice(deviceID);
}

int cc_getWidth(int deviceID)
{
    return VI.getWidth(deviceID);
}

int cc_getHeight(int deviceID)
{
    return VI.getHeight(deviceID);
}

bool cc_isFrameNew(int deviceID)
{
    return VI.isFrameNew(deviceID);
}

bool cc_getPixels(int deviceID, unsigned char* pixels)
{
    return VI.getPixels(deviceID, pixels, true);
}
