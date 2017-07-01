#include <GL/gl3w.h>

#include "render.h"
#include "logging.h"

int screenWidth;
int screenHeight;

bool Render::Setup()
{
    if(gl3wInit())
    {
        logFail("Unable to initialize OpenGL\n");
        return false;
    }

    logInfo("Initialized OpenGL %s with support for GLSL %s\n",
            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    return true;
}

void Render::Shutdown()
{
    logInfo("Deinitialize graphics subsystem\n");
}

void Render::updateWindowSize(int newWidth, int newHeight)
{
    screenWidth = newWidth;
    screenHeight = newHeight;
    glViewport(0,0, newWidth, newHeight);
}

GLuint Render::createTexture()
{
    GLuint result;
    glGenTextures(1, &result);
    glBindTexture(GL_TEXTURE_2D, result);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    return result;
}

void Render::glPrintError(bool alwaysPrint)
{
    GLenum error = glGetError();
    if(error == GL_NO_ERROR)
    {
        if(alwaysPrint)
        {
            logInfo("OpenGL error: GL_NO_ERROR\n");
        }
        return;
    }

    const char* errorStr;
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

    logWarn("OpenGL error: %s\n", errorStr);
}
