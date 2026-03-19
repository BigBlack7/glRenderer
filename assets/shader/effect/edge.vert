#version 460 core
layout(location = 0)in vec3 lPosition;

// instancing attributes
layout(location = 3)in vec4 iM0;
layout(location = 4)in vec4 iM1;
layout(location = 5)in vec4 iM2;
layout(location = 6)in vec4 iM3;

layout(std140, binding = 0)uniform FrameBlock
{
    mat4 uV;
    mat4 uP;
    vec4 uViewPosTime;
};

// uniform
uniform mat4 uM;
uniform uint uUseInstancing = 0u;

void main()
{
    mat4 model = (uUseInstancing != 0u) ? mat4(iM0, iM1, iM2, iM3) : uM;
    gl_Position = uP * uV * model * vec4(lPosition, 1.0);
}