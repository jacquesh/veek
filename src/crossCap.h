#ifndef _CROSS_CAP_H
#define _CROSS_CAP_H

void cc_initialize();
int cc_deviceCount();
const char* cc_deviceName(int deviceID);
bool cc_enableDevice(int deviceID, int width, int height);
void cc_disableDevice(int deviceID);
int cc_getWidth(int deviceID);
int cc_getHeight(int deviceID);
bool cc_isFrameNew(int deviceID);
bool cc_getPixels(int deviceID, unsigned char* pixels);

#endif
