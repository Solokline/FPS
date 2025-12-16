#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform mat4 ortho;

void main()
{
    gl_Position = ortho * vec4(aPos, 0.0, 1.0);
    TexCoord = aTex;
}