#version 330 core

in vec4 color;

layout (location = 0) out vec4 out_color;
in float dist;
uniform int dash;
uniform float time;

void main()
{
    out_color = color;
    if (dash == 1 && mod(dist - time * 10, 40.0) < 20.0)
    discard;
}