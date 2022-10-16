#version 330 core

uniform mat4 view;

layout (location = 0) in vec2 in_position;

void main()
{
    gl_Position = view * vec4(in_position, 0.0, 1.0);
}