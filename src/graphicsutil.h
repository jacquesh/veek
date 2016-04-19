#ifndef _GRAPHICSUTIL_H
#define _GRAPHICSUTIL_H

#include <GL/gl3w.h>

#define glPrintError(alwaysPrint) __glPrintError(__FILE__, __LINE__, alwaysPrint)
void __glPrintError(const char* file, int line, bool alwaysPrint);

GLuint loadShader(const char* shaderFilename, GLenum shaderType);
GLuint loadShaderProgram(const char* vertShaderFilename, const char* fragShaderFilename);
GLuint loadShaderProgram(const char* vertShaderFilename,
                         const char* geomShaderFilename,
                         const char* fragShaderFilename);

#endif
