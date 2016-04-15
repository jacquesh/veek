#version 330 core

in vec2 fragTexCoord;

uniform vec4 colorTint;
uniform sampler2D spriteTex;

out vec4 outColor;

void main()
{
    vec4 textureColor = texture(spriteTex, fragTexCoord);
    outColor = colorTint * textureColor;
}
