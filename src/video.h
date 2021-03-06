#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdint.h>

#include "common.h"
#include "user.h"

const int cameraWidth = 320;
const int cameraHeight = 240;

namespace Video
{
    struct NetworkVideoPacket
    {
        UserIdentifier srcUser;
        uint8 index;
        uint16 imageWidth;
        uint16 imageHeight;
        uint16 encodedDataLength;
        uint8 encodedData[cameraWidth*cameraHeight*3]; // TODO: Sizing

        template<typename Packet> bool serialize(Packet& packet);
    };

    bool Setup();
    void Update();
    void Shutdown();

    /**
     * \return The new state of the camera device, which is equal to enabled if the function succeeded,
     * and equal to the previous state of the device if the function failed
     */
    bool enableCamera(int deviceID);
    bool checkForNewVideoFrame();
    uint8_t* currentVideoFrame();

    int encodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer);
    int decodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer);
}

#endif
