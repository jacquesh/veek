#ifndef _RENDER_H
#define _RENDER_H

#include <stdio.h>

#include <SDL.h>
#include <GL/gl3w.h>

#include "vecmath.h"

extern int screenWidth;
extern int screenHeight;

#define glPrintError(alwaysPrint) __glPrintError(__FILE__, __LINE__, alwaysPrint)
static inline void __glPrintError(const char* file, int line, bool alwaysPrint)
{
  GLenum error = glGetError();
  const char* errorStr;
  if(alwaysPrint || (error != GL_NO_ERROR))
  {
      switch(error)
      {
      case GL_NO_ERROR:
          errorStr = "GL_NO_ERROR";
          break;
      case GL_INVALID_ENUM:
          errorStr = "GL_INVALID_ENUM";
          break;
      case GL_INVALID_VALUE:
          errorStr = "GL_INVALID_VALUE";
          break;
      case GL_INVALID_OPERATION:
          errorStr = "GL_INVALID_OPERATION";
          break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
          errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION";
          break;
      case GL_OUT_OF_MEMORY:
          errorStr = "GL_OUT_OF_MEMORY";
          break;
      default:
          errorStr = "UNRECOGNIZED";
          break;
      }

      fprintf(stderr, "(%s:%d) OpenGL error: %s\n", file, line, errorStr);
  }
}

GLuint loadShader(const char* shaderFilename, GLenum shaderType);
GLuint loadShaderProgram(const char* vertShaderFilename, const char* fragShaderFilename);
GLuint loadShaderProgram(const char* vertShaderFilename,
                         const char* geomShaderFilename,
                         const char* fragShaderFilename);

void loadDefaultShaders();
void updateWindowSize(int newWidth, int newHeight);

void renderTexture(GLuint textureID, Vector2 position, Vector2 size, float opacity);

#endif
