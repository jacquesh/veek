#include "graphicsutil.h"

#include <stdio.h>
#include <stdlib.h>

#include <GL/gl3w.h>
#include <SDL_rwops.h>

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

GLuint loadShader(const char* shaderFilename, GLenum shaderType)
{
    SDL_RWops* shaderFile = SDL_RWFromFile(shaderFilename, "r");
    if(shaderFile == nullptr)
    {
        printf("Error while trying to open shader file: %s\n", SDL_GetError());
        return 0;
    }

    int64_t shaderSize = SDL_RWseek(shaderFile, 0, RW_SEEK_END);
    SDL_RWseek(shaderFile, 0, RW_SEEK_SET);
    char* shaderText = (char*)malloc(shaderSize+1);

    // TODO: Handle the case where it doesn't read the entire shader at once, only a part of it
    //       IE we should probably loop here until it reads the entire file
    int64_t bytesRead = SDL_RWread(shaderFile, shaderText, 1, shaderSize);
    if(bytesRead != shaderSize)
    {
        printf("Expected to read %ld bytes from %s, instead got %ld\n", shaderSize, shaderFilename, bytesRead);
        free(shaderText);
        SDL_RWclose(shaderFile);
        return 0;
    }

    shaderText[shaderSize] = '\0';
    SDL_RWclose(shaderFile);

    // TODO: Handle GLerrors (Do we actually need to even?)
    //       If we have a problem here we'll catch it when we try to link the shader anyways
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderText, NULL);
    glCompileShader(shader);

    free(shaderText);

    return shader;
}

GLuint loadShaderProgram(const char* vertShaderFilename,
                         const char* fragShaderFilename)
{
    GLuint vertShader = loadShader(vertShaderFilename, GL_VERTEX_SHADER);
    GLuint fragShader = loadShader(fragShaderFilename, GL_FRAGMENT_SHADER);

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

    printf("Successfully loaded shader from %s and %s\n", vertShaderFilename, fragShaderFilename);
    return program;
}

GLuint loadShaderProgram(const char* vertShaderFilename,
                         const char* geomShaderFilename,
                         const char* fragShaderFilename)
{
    GLuint vertShader = loadShader(vertShaderFilename, GL_VERTEX_SHADER);
    GLuint geomShader = loadShader(geomShaderFilename, GL_GEOMETRY_SHADER);
    GLuint fragShader = loadShader(fragShaderFilename, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, geomShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    glDeleteShader(vertShader);
    glDeleteShader(geomShader);
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

    printf("Successfully loaded shader from %s, %s and %s\n",
            vertShaderFilename, geomShaderFilename, fragShaderFilename);
    return program;
}
