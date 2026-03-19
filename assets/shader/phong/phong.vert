#version 460 core
layout(location = 0)in vec3 lPosition;
layout(location = 1)in vec2 lUV;
layout(location = 2)in vec3 lNormal;

// instancing attributes
layout(location = 3)in vec4 iM0;
layout(location = 4)in vec4 iM1;
layout(location = 5)in vec4 iM2;
layout(location = 6)in vec4 iM3;

layout(location = 7)in vec4 iN0;
layout(location = 8)in vec4 iN1;
layout(location = 9)in vec4 iN2;

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
uniform uint uUseInstancing = 0u;

void main()
{
    mat4 model = uM;
    mat3 normalMat = uN;
    
    if (uUseInstancing != 0u)
    {
        model = mat4(iM0, iM1, iM2, iM3);
        normalMat = mat3(iN0.xyz, iN1.xyz, iN2.xyz);
    }
    
    vec4 worldPos = model * vec4(lPosition, 1.0);
    
    gl_Position = uP * uV * worldPos;
    oUV = lUV;
    oNormal = normalize(normalMat * lNormal);
    oFragPos = worldPos.xyz;
}