#version 330 core
layout (location = 0) out vec4 out_color;

uniform sampler2D environment_map;
uniform vec3 camera_position;
uniform float brightness;

in vec3 position;

const float PI = 3.141592653589793;

void main()
{
    vec3 dir = normalize(position - camera_position);
    float x = atan(dir.z, dir.x) / PI * 0.5 + 0.5;
    float y = -atan(dir.y, length(dir.xz)) / PI + 0.5;

    vec3 albedo = texture(environment_map, vec2(x, y)).rgb;
    out_color = vec4(albedo * brightness, 1.0);
}