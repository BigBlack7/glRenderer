#version 460 core
layout(location = 0)in vec3 lPosition;

uniform mat4 uVP;
uniform mat4 uM;

void main()
{
    gl_Position = uVP * uM * vec4(lPosition, 1.0);
}
