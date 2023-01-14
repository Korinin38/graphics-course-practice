#version 330 core

uniform vec3 light_direction;
uniform vec3 camera_position;
uniform float brightness;
uniform float reflectiveness;

uniform sampler2D albedo_texture;
uniform sampler2D reflection_map;

in vec3 position;
in vec3 tangent;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

const float PI = 3.141592653589793;
const float n = 4;

void main()
{
    float ambient_light = 0.2;

    float lightness = ambient_light + max(0.0, dot(normalize(normal), light_direction));
    vec3 cam_dir = normalize(camera_position - position);
    vec3 dir = 2.0 * normal * dot(normal, cam_dir) - cam_dir;
    float x = atan(dir.z, dir.x) / PI * 0.5 + 0.5;
    float y = -atan(dir.y, length(dir.xz)) / PI + 0.5;


//    vec3 albedo = vec3(1.0);
    vec3 reflection = texture(reflection_map, vec2(x, y)).rgb;
    vec3 albedo = texture(albedo_texture, texcoord).rgb;
//    vec3 albedo = normal * 0.2 + vec3(0.8);
//    vec3 albedo = texture(normal_texture, texcoord).rgb;
//    vec3 albedo = normal * 0.5 + vec3(0.5);

    albedo *= brightness;

    float refl = max(reflectiveness, 0.1);
//    out_color = vec4(lightness * mix(albedo, reflection, 0.5), 1 - opacity);
    out_color = vec4(lightness * mix(albedo, reflection, refl), 1);
}