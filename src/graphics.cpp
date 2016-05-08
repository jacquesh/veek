#include "graphics.h"

#include "common.h"
#include "graphicsutil.h"
#include "vecmath.h"

GLuint spriteVAO;
GLuint spriteShader;
GLuint indexBuffer;
GLuint vertexLocBuffer;
GLuint texCoordBuffer;

int screenWidth;
int screenHeight;

#include "shaders.cpp"

bool initGraphics()
{
    if(gl3wInit())
    {
        log("Unable to initialize OpenGL\n");
        return false;
    }

    // Load shader
    glGenVertexArrays(1, &spriteVAO);
    spriteShader = loadShaderProgramFromString(vertexShader, fragmentShader);
    GLint spritePositionLoc = glGetAttribLocation(spriteShader, "position");
    GLint spriteTexCoordLoc = glGetAttribLocation(spriteShader, "texCoord");

    // Load VBO/VAO data
    float vertexLocData[12] = {-0.5f, -0.5f, 0.0f,
                               0.5f, -0.5f, 0.0f,
                               0.5f,  0.5f, 0.0f,
                              -0.5f,  0.5f, 0.0f};
    float texCoordData[8] = {0.0f, 0.0f,
                             1.0f, 0.0f,
                             1.0f, 1.0f,
                             0.0f, 1.0f};
    GLuint indexData[6] = {0,1,3,
                           1,2,3};

    glBindVertexArray(spriteVAO);
    glGenBuffers(1, &vertexLocBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexLocBuffer);
    glBufferData(GL_ARRAY_BUFFER, 4*3*sizeof(float), vertexLocData,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(spritePositionLoc);
    glVertexAttribPointer(spritePositionLoc, 3, GL_FLOAT, false, 0, 0);

    glGenBuffers(1, &texCoordBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, texCoordBuffer);
    glBufferData(GL_ARRAY_BUFFER, 4*2*sizeof(float), texCoordData,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(spriteTexCoordLoc);
    glVertexAttribPointer(spriteTexCoordLoc, 2, GL_FLOAT, false, 0, 0);

    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6*sizeof(int), indexData, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    log("Initialized OpenGL %s with support for GLSL %s\n",
            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    return true;
}

void deinitGraphics()
{
}

void updateWindowSize(int newWidth, int newHeight)
{
    screenWidth = newWidth;
    screenHeight = newHeight;
    glViewport(0,0, newWidth, newHeight);
}

void renderTexture(GLuint textureID, Vector2 position, Vector2 size, float opacity)
{
    glUseProgram(spriteShader);

    float projectionMatrix[16] = {2.0f/screenWidth,0,0,0,
                                  0,2.0f/screenHeight,0,0,
                                  0,0,-1.0f,0,
                                  0,0,-1,1};
    float viewingMatrix[16] = {1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                -screenWidth/2.0f, -screenHeight/2.0f, 0, 1};

    GLint projectionMatrixLoc = glGetUniformLocation(spriteShader, "projectionMatrix");
    GLint viewingMatrixLoc = glGetUniformLocation(spriteShader, "viewingMatrix");
    glUniformMatrix4fv(projectionMatrixLoc, 1, false, projectionMatrix);
    glUniformMatrix4fv(viewingMatrixLoc, 1, false, viewingMatrix);

    float modelMatrix[16] = {size.x, 0, 0, 0,
                             0, size.y, 0, 0,
                             0, 0, 1, 0,
                             position.x, position.y, 0, 1};

    GLint colorTintLoc = glGetUniformLocation(spriteShader, "colorTint");
    GLint textureSamplerLoc = glGetUniformLocation(spriteShader, "spriteTex");
    GLint modelMatrixLoc = glGetUniformLocation(spriteShader, "modelMatrix");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(textureSamplerLoc, 0);

    glUniform4f(colorTintLoc, 1.0f, 1.0f, 1.0f, opacity);

    glUniformMatrix4fv(modelMatrixLoc, 1, false, modelMatrix);

    glBindVertexArray(spriteVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);
    glUseProgram(0);
}
