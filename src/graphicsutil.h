#ifndef _GRAPHICSUTIL_H
#define _GRAPHICSUTIL_H

#include <GL/gl3w.h>

void glPrintError(bool alwaysPrint);

GLuint loadShaderFromString(const char* shaderStr, GLenum shaderType);
GLuint loadShaderProgramFromString(const char* vertShaderStr, const char* fragShaderStr);

#endif
