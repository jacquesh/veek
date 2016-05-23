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

int encodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer);
int decodeRGBImage(int inputLength, uint8* inputBuffer, int outputLength, uint8* outputBuffer);

#endif
