#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdint.h>

bool initVideo();
void deinitVideo();

/**
 * \return The new state of the camera device, which is equal to enabled if the function succeeded,
 * and equal to the previous state of the device if the function failed
 */
bool enableCamera(int deviceID);
bool checkForNewVideoFrame();
uint8_t* currentVideoFrame();

int encodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer);
int decodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer);

#endif
