#version 460 core
layout(location = 0)in vec3 vPos;
layout(location = 1)in vec3 vColor;
layout(location = 2)in vec2 vUV;

// out
out vec3 color;
out vec2 uv;

// uniform
uniform mat4 transform;

void main()
{
    gl_Position = transform * vec4(vPos, 1.0);
    color = vColor;
    uv = vUV;
}