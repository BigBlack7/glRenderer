#version 460 core
layout(location = 0)in vec3 vPos;
layout(location = 1)in vec3 vColor;

// out
out vec3 color;

// uniform
uniform float time;

void main()
{
    gl_Position = vec4(vPos + vec3(sin(time * 10.0) * 0.3), 1.0);
    color = vColor;
}