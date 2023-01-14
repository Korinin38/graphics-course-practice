#version 330 core

uniform vec3 light_direction;
uniform mat4 transform;
uniform sampler2D albedo;
uniform sampler2D shadow_map;
uniform float bias;
uniform float brightness;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

void main()
{
//    vec4 shadow_pos = transform * vec4(position, 1.0);
//    shadow_pos /= shadow_pos.w;
//    shadow_pos = shadow_pos * 0.5 + vec4(0.5);


//    vec3 sum = vec3(0.0);
//    float sum_w = 0.0;
//    const int N = 5;
//    float radius = 3.0;
//    for (int x = -N; x <= N; ++x)
//    {
//        for (int y = -N; y <= N; ++y)
//        {
//            vec2 data = texture(shadow_map, shadow_pos.xy + vec2(x,y) / textureSize(shadow_map, 0)).rg;
//            float c = exp(-float(x*x + y*y) / (radius*radius));
//            float mu = data.r;
//            float sigma = data.g - mu * mu;
//            float z = shadow_pos.z - bias;
//            sigma += 0.25 * (dFdx(z) * dFdx(z) + dFdy(z) * dFdy(z));
//            float factor = (z < mu) ? 1.0 : sigma / (sigma + (z - mu) * (z - mu));
//            float delta = 0.1;
//            factor = factor < delta ? 0 : (factor - delta) / (1 - delta);
//            sum += c * max(0.0, dot(normal, light_direction)) * factor;
//            sum_w += c;
//        }
//    }

    float light = max(brightness * 0.3 + 0.7, 0.7);
//    light += sum / sum_w;
//    vec3 color = texture(albedo, texcoord).xyz * light;
    vec3 color = texture(albedo, texcoord).xyz * light;
//    vec3 color = vec3(1.0, 1.0, 1.0) * light;

    out_color = vec4(color, 1.0);
}