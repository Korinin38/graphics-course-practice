#version 330 core

uniform vec3 light_direction;
uniform vec3 camera_position;
uniform float brightness;

uniform sampler2D normal_texture;

in vec3 position;
in vec3 tangent;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

const float PI = 3.141592653589793;

void main()
{
    float ambient_light = brightness;

    vec3 bitangent = cross (tangent, normal);
    mat3 tbn = mat3(tangent, bitangent, normal);
    vec3 real_normal = tbn * (texture(normal_texture, texcoord).xyz * 2.0 - vec3(1.0));
    real_normal = normalize(mix(normal, real_normal, 0.5));

    //    float lightness = ambient_light + max(0.0, dot(normalize(normal), light_direction));
    float lightness = ambient_light + max(0.0, dot(normalize(real_normal), light_direction));
    vec3 cam_dir = normalize(camera_position - position);
    vec3 dir = 2.0 * real_normal * dot(real_normal, cam_dir) - cam_dir;
    float x = atan(dir.z, dir.x) / PI * 0.5 + 0.5;
    float y = -atan(dir.y, length(dir.xz)) / PI + 0.5;


    vec3 albedo = vec3(0.8, 0.8, 1.0);

    out_color = vec4(lightness * albedo, 1.0);
}