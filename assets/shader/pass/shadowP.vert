#version 460 core
layout(location = 0)in vec3 lPosition;

// instancing attributes
layout(location = 4)in vec4 iM0;
layout(location = 5)in vec4 iM1;
layout(location = 6)in vec4 iM2;
layout(location = 7)in vec4 iM3;

uniform mat4 uM;
uniform mat4 uLightVP;
uniform uint uUseInstancing = 0u;

out vec3 oFragPos;

void main()
{
    mat4 model = uM;
    if (uUseInstancing != 0u)model = mat4(iM0, iM1, iM2, iM3);
    
    vec4 worldPos = model * vec4(lPosition, 1.0);
    
    gl_Position = uLightVP * worldPos;
    
    oFragPos = worldPos.xyz;
}