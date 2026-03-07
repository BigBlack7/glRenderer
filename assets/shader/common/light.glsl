#ifndef MAX_DIRECTIONAL_LIGHTS
#define MAX_DIRECTIONAL_LIGHTS 4
#endif

struct DirectionalLightGpu
{
    vec4 direction; // xyz: direction
    vec4 colorIntensity; // rgb: color, a: intensity
};

layout(std140, binding = 1)uniform LightBlock
{
    ivec4 uDirectionalLightMeta; // x = directional light count
    DirectionalLightGpu uDirectionalLights[MAX_DIRECTIONAL_LIGHTS];
};