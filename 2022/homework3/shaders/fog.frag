#version 330 core

uniform sampler3D cloud_texture;
uniform vec3 camera_position;
uniform vec3 light_direction;
uniform vec3 bbox_min;
uniform vec3 bbox_max;
uniform vec3 centre;

layout (location = 0) out vec4 out_color;

void sort(inout float x, inout float y)
{
    if (x > y)
    {
        float t = x;
        x = y;
        y = t;
    }
}

float vmin(vec3 v)
{
    return min(v.x, min(v.y, v.z));
}

float vmax(vec3 v)
{
    return max(v.x, max(v.y, v.z));
}

vec2 intersect_bbox(vec3 origin, vec3 direction)
{
    vec3 tmin = (bbox_min - origin) / direction;
    vec3 tmax = (bbox_max - origin) / direction;

    sort(tmin.x, tmax.x);
    sort(tmin.y, tmax.y);
    sort(tmin.z, tmax.z);

    return vec2(vmax(tmin), vmin(tmax));
}

float tex_from_space(vec3 pos)
{
    return texture(cloud_texture, (pos - bbox_min) / (bbox_max - bbox_min)).x;
}

const float PI = 3.1415926535;
const vec3 absorption = vec3(0.3, 0.3, 0.3);
const vec3 scattering = vec3(4.0, 4.0, 4.0);
const vec3 extinction = absorption + scattering;
const vec3 light_color = vec3(16.0);
const int N = 32;
const int M = 8;

in vec3 position;

void main()
{
//
//    if (distance(position, centre) >= 1.05) {
//
//    }
    vec3 direction = normalize(position - camera_position);
    vec2 intersect_interval = intersect_bbox(camera_position, direction);
    float tmin = intersect_interval.x;
    float tmax = intersect_interval.y;
    tmin = max(tmin, 0.0);

    vec3 optical_depth = vec3(0);
    vec3 color = vec3(0.0);
    for (int i = 0; i < N; ++i)
    {
        float dt = (tmax - tmin) / N;
        float t = tmin + (i + 0.5) * dt;
        vec3 p = camera_position + t * direction;
        float density = tex_from_space(p);
        optical_depth += extinction * density * dt;

        vec3 light_optical_depth = vec3(0);
        vec2 ib = intersect_bbox(p, light_direction);
        ib.x = max(ib.x, 0.0);
        float ds = (ib.y - ib.x) / M;
//        for (int j = 0; j < M; ++j)
//        {
//            float s = ib.x + (j + 0.5) * ds;
//            vec3 q = p + s * light_direction;
//            light_optical_depth += extinction * tex_from_space(q) * ds;
//        }
        color += light_color * exp(- light_optical_depth - optical_depth) * dt * density * scattering / 4.0 / PI;
    }
    float opacity = 0.6 - exp(-optical_depth.x);
//    float opacity = 1.0;
//    vec3 color = vec3(0.0);
    out_color = vec4(color, opacity);
}