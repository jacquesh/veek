#ifndef _RENDER_H
#define _RENDER_H

#include <GL/gl3w.h> // Exclusively for GLuint

extern int screenWidth;
extern int screenHeight;

bool initGraphics();
void deinitGraphics();

void updateWindowSize(int newWidth, int newHeight);
GLuint createTexture();

void glPrintError(bool alwaysPrint);

#endif
