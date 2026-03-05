#ifndef MAX_DIRECTIONAL_LIGHTS
#define MAX_DIRECTIONAL_LIGHTS 4
#endif

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
};

uniform int uDirectionalLightCount;
uniform DirectionalLight uDirectionalLights[MAX_DIRECTIONAL_LIGHTS];