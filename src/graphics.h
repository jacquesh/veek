#ifndef _RENDER_H
#define _RENDER_H

extern int screenWidth;
extern int screenHeight;

bool initGraphics();
void deinitGraphics();

void updateWindowSize(int newWidth, int newHeight);

void glPrintError(bool alwaysPrint);

#endif
