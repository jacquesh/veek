#include "graphicsutil.h"

#include <stdio.h>
#include <stdlib.h>

#include <GL/gl3w.h>

void __glPrintError(const char* file, int line, bool alwaysPrint)
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

GLuint loadShaderFromString(const char* shaderStr, GLenum shaderType)
{
    // TODO: Handle GLerrors (Do we actually need to even?)
    //       If we have a problem here we'll catch it when we try to link the shader anyways
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderStr, NULL);
    glCompileShader(shader);

    return shader;
}

GLuint loadShaderProgramFromString(const char* vertShaderStr,
                                   const char* fragShaderStr)
{
    GLuint vertShader = loadShaderFromString(vertShaderStr, GL_VERTEX_SHADER);
    GLuint fragShader = loadShaderFromString(fragShaderStr, GL_FRAGMENT_SHADER);

    // TODO: Handle GLerrors
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if(linkStatus != GL_TRUE)
    {
        GLsizei logLength = 0;
        GLchar message[1024];
        glGetProgramInfoLog(program, 1024, &logLength, message);
        printf("Error: %s\n", message);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}
