#version 330 core

out vec4 something;
void main()
{
    float z = gl_FragCoord.z;
    something = vec4(z, z * z, 0.0, 0.0);
}
