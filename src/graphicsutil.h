#ifndef _GRAPHICSUTIL_H
#define _GRAPHICSUTIL_H

#include <GL/gl3w.h>

#define glPrintError(alwaysPrint) __glPrintError(__FILE__, __LINE__, alwaysPrint)
void __glPrintError(const char* file, int line, bool alwaysPrint);

GLuint loadShaderFromString(const char* shaderStr, GLenum shaderType);
GLuint loadShaderProgramFromString(const char* vertShaderStr, const char* fragShaderStr);

#endif
