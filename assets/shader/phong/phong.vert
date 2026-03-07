#version 460 core
layout(location = 0)in vec3 lPosition;
layout(location = 1)in vec2 lUV;
layout(location = 2)in vec3 lNormal;

// out
out vec2 oUV;
out vec3 oNormal;
out vec3 oFragPos;

layout(std140, binding = 0)uniform FrameBlock
{
    mat4 uV;
    mat4 uP;
    vec4 uViewPosTime;
};

// uniform
uniform mat4 uM;
uniform mat3 uN;

void main()
{
    gl_Position = uP * uV * uM * vec4(lPosition, 1.0);
    oUV = lUV;
    oNormal = uN * lNormal;
    oFragPos = (uM * vec4(lPosition, 1.0)).xyz;
}