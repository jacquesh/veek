#include "render.h"

#include <SDL.h>
#include <SDL_rwops.h>

#include <GL/gl3w.h>

#include "vecmath.h"

GLuint spriteVAO;
GLuint spriteShader;
GLuint indexBuffer;
GLuint vertexLocBuffer;
GLuint texCoordBuffer;

int screenWidth;
int screenHeight;

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

void loadDefaultShaders()
{
    // Load shaders
    // Sprite Shader
    glGenVertexArrays(1, &spriteVAO);
    spriteShader = loadShaderProgram("resources/shaders/sprite.vsh",
                                          "resources/shaders/sprite.fsh");
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
}

void updateWindowSize(int newWidth, int newHeight)
{
    screenWidth = newWidth;
    screenHeight = newHeight;
    glViewport(0,0, newWidth, newHeight);
}

/*
 * Renders a sprite at the given position and size and rotation (in radians).
 * The specified colour is used to tint the sprite (use white if no tinting is desired)
*/
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
