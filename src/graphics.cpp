#include <GL/gl3w.h>

#include "graphics.h"
#include "logging.h"

int screenWidth;
int screenHeight;

bool initGraphics()
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

void deinitGraphics()
{
    logInfo("Deinitialize graphics subsystem\n");
}

void updateWindowSize(int newWidth, int newHeight)
{
    screenWidth = newWidth;
    screenHeight = newHeight;
    glViewport(0,0, newWidth, newHeight);
}

void glPrintError(bool alwaysPrint)
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
