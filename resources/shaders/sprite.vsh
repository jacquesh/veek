#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;

uniform mat4 projectionMatrix;
uniform mat4 viewingMatrix;
uniform mat4 modelMatrix;

out vec2 fragTexCoord;

void main()
{
    vec4 transformedPosition = projectionMatrix*viewingMatrix*modelMatrix*vec4(position,1.0f);
    gl_Position = transformedPosition;
    fragTexCoord = texCoord;
}
