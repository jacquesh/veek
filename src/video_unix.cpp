#include "video.h"

bool Video::Setup()
{
    return true;
}

void Video::Shutdown()
{
}

bool enableCamera(int deviceId)
{
    return false;
}

bool checkForNewVideoFrame()
{
    return false;
}

uint8_t* currentVideoFrame()
{
    return nullptr;
}

int Video::encodeRGBImage(int inputLength, uint8_t* inputBuffer,
                          int outputLength, uint8_t* outputBuffer)
{
    return 0;
}

int Video::decodeRGBImage(int inputLength, uint8_t* inputBuffer,
                          int outputLength, uint8_t* outputBuffer)
{
    return 0;
}

