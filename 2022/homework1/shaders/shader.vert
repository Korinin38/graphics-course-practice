#version 330 core

uniform mat4 view;
uniform float max_value;

layout (location = 0) in vec2 in_position;
layout (location = 1) in float value;

out vec4 color;

void main()
{
    float _val = value;
    gl_Position = view * vec4(in_position, 0.0, 1.0);
    if (value < 0) {
        float val = -max(value, -max_value) / max_value;
        color = vec4(1 - val, 1 - val, 1.0, 1.0);
    }
    else {
        float val = min(value, max_value) / max_value;
        color = vec4(1.0, 1 - val, 1 - val, 1.0);
    }
}