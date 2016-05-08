#ifndef _RENDER_H
#define _RENDER_H

#include <GL/gl3w.h>

#include "vecmath.h"

extern int screenWidth;
extern int screenHeight;

bool initGraphics();
void deinitGraphics();

void updateWindowSize(int newWidth, int newHeight);

void renderTexture(GLuint textureID, Vector2 position, Vector2 size, float opacity);

#endif
