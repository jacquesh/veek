#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdint.h>

// TODO: Remove
extern int cameraWidth;
extern int cameraHeight;

bool initVideo();
void deinitVideo();

void enableCamera(bool enabled);
bool checkForNewVideoFrame();
uint8_t* currentVideoFrame();

void encodeRGBImage(uint8* inputData);

#endif
