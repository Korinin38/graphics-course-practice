#version 330 core

uniform vec3 camera_position;

//uniform vec3 albedo;
uniform sampler2D albedo;
uniform sampler2D transparency;
uniform int solid;

uniform vec3 sun_direction;
uniform vec3 sun_color;

uniform sampler2D shadow_map;
uniform mat4 transform;
uniform float bias;

uniform vec3 glossiness;
uniform float roughness;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

vec3 diffuse(vec3 direction) {
    return texture(albedo, texcoord).xyz  * max(0.0, dot(normal, direction));
}

vec3 specular(vec3 direction) {
    float power = 1.0 / (roughness * roughness + 1) - 1.0;
    vec3 reflected_direction = 2.0 * normal * dot(normal, direction) - direction;
//    vec3 view_direction = camera_position;
    vec3 view_direction = -normalize(camera_position - position);
    return glossiness.x * sun_color * pow(max(0.0, dot(reflected_direction, view_direction)), power) ;
}

//vec3 specular(vec3 direction)
//{
//    vec3 reflected = 2.0 * normal * dot(normal, direction) - direction;
//    float power = 1.0 / (roughness * roughness) - 1.0;
//    return glossiness * albedo * pow(max(0.0, dot(reflected, normalize(camera_position - position))), power);
//}

vec3 phong(vec3 direction) {
//    return diffuse(direction) + specular(direction);
//    return diffuse(direction);
    return specular(direction);
}

void main()
{
    bool transparent = (solid == 0);
//    if (texture(transparency, texcoord).x < 0.5)
//        discard;
    if (transparent && texture(transparency, texcoord).x < 0.5)
        discard;
    float ambient_light = 0.2;
//    float ambient_light = 1;

    vec3 light = vec3(ambient_light);
//    vec4 ndc = shadow_projection * vec4(position, 1.0);
//    bool in_shadow_texture = (-1.0 <= ndc.x && ndc.x <= 1.0 && -1.0 <= ndc.y && ndc.y <= 1.0 && -1.0 <= ndc.z && ndc.z <= 1.0);
//    bool in_shadow_texture = false;
//    ndc = ndc * 0.5 + vec4(0.5);
//
//    vec3 sum = vec3(0.0);
//    float sum_w = 0.0;
//    const int N = 5;
//    float radius = 3.0;
//    for (int x = -N; x <= N; ++x)
//    {
//        for (int y = -N; y <= N; ++y)
//        {
//            float in_shadow = texture(shadow_map, ndc.xyz + vec3(x,y,0) / vec3(textureSize(shadow_map, 0), 1));
//            float c = exp(-float(x*x + y*y) / (radius*radius));
//            if (!in_shadow_texture || (in_shadow == 1))
//            {
//                sum += c * sun_color * phong(sun_direction);
//            }
//            sum_w += c;
//        }
//    }

//    color += sum / sum_w;

    vec4 shadow_pos = transform * vec4(position, 1.0);
    shadow_pos /= shadow_pos.w;
    shadow_pos = shadow_pos * 0.5 + vec4(0.5);

    vec3 sum = vec3(0.0);
    float sum_w = 0.0;
    const int N = 5;
    float radius = 3.0;
    for (int x = -N; x <= N; ++x)
    {
        for (int y = -N; y <= N; ++y)
        {
            vec2 data = texture(shadow_map, shadow_pos.xy + vec2(x,y) / textureSize(shadow_map, 0)).rg;
            float c = exp(-float(x*x + y*y) / (radius*radius));
            float mu = data.r;
            float sigma = data.g - mu * mu;
            float z = shadow_pos.z - bias;
            sigma += 0.25 * (dFdx(z) * dFdx(z) + dFdy(z) * dFdy(z));
            float factor = (z < mu) ? 1.0 : sigma / (sigma + (z - mu) * (z - mu));
            float delta = 0.1;
            factor = factor < delta ? 0 : (factor - delta) / (1 - delta);
            sum += c * phong(sun_direction) * max(0.0, dot(normal, sun_direction)) * factor;
            sum_w += c;
        }
    }

    light += phong(sun_direction);

//    light += sum / sum_w;

    vec4 tex_source = texture(albedo, texcoord);
    vec3 color = (tex_source.xyz / tex_source.w) * light;

    out_color = vec4(color, 1.0);
}
