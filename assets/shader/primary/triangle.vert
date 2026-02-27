#version 460 core
layout(location = 0)in vec3 vPos;
layout(location = 1)in vec2 vUV;

// out
out vec2 uv;

// uniform
uniform mat4 M;
uniform mat4 V;
uniform mat4 P;

void main()
{
    gl_Position = P * V * M * vec4(vPos, 1.0);
    uv = vUV;
}