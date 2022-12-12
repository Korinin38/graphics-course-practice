#version 330 core

uniform vec3 light_direction;
uniform float brightness;

uniform sampler2D normal_texture;
uniform sampler2D shadow_map;
uniform mat4 transform;

in vec3 tangent;
in vec3 normal;
in vec2 texcoord;
in vec3 position;

layout (location = 0) out vec4 out_color;

const float PI = 3.141592653589793;
const float bias = 0.01;

vec3 shadow()
{
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
            sum += c * max(0.0, dot(normal, light_direction)) * factor;
            sum_w += c;
        }
    }


    return sum / sum_w;
}

void main()
{
    float ambient_light = brightness * 0.6;

    vec3 bitangent = cross (tangent, normal);
    mat3 tbn = mat3(tangent, bitangent, normal);
    vec3 real_normal = tbn * (texture(normal_texture, texcoord).xyz * 2.0 - vec3(1.0));
    real_normal = normalize(mix(normal, real_normal, 0.5));

    float lightness = ambient_light;
    vec3 lighting = vec3(lightness) + max(0.0, dot(real_normal, light_direction)) * shadow();

    vec3 albedo = vec3(0.8, 0.8, 1.0);
//    vec3 albedo = real_normal * 0.5 + vec3(0.5);

    out_color = vec4(lighting * albedo, 1.0);
}