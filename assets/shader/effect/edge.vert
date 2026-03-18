#version 460 core
layout(location = 0)in vec3 lPosition;

layout(std140, binding = 0)uniform FrameBlock
{
    mat4 uV;
    mat4 uP;
    vec4 uViewPosTime;
};

// uniform
uniform mat4 uM;

void main()
{
    gl_Position = uP * uV * uM * vec4(lPosition, 1.0);
}