#ifndef MAX_DIRECTIONAL_LIGHTS
#define MAX_DIRECTIONAL_LIGHTS 4
#endif

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 16
#endif

#ifndef MAX_SPOT_LIGHTS
#define MAX_SPOT_LIGHTS 8
#endif

const float EPS = 1e-5;
const float specularIntensity = 0.3;
const float PI = 3.14159265359;

struct DirectionalLightGPU
{
    vec4 direction; // xyz: direction
    vec4 colorIntensity; // rgb: color, a: intensity
    vec4 shadowParams0; // x: enabled, y: technique, z: biasConstant, w: biasSlope
    vec4 shadowParams1; // x: pcfRadiusTexels, y: poissonRadiusUV, z: poissonSamples, w: cascadeSplitExponent(CSM)
    vec4 shadowParams2; // x: pcssBlockerSearchTexels, y: pcssLightSizeTexels, z: pcssMinFilterTexels, w: pcssMaxFilterTexels
    vec4 shadowParams3; // x: pcssBlockerSamples, y: pcssFilterSamples, z: cascadeCount, w: cascadeBlend
};

struct PointLightGPU
{
    vec4 positionRange; // xyz: position, w: range
    vec4 colorIntensity; // rgb: color, a: intensity
    vec4 attenuation; // x: constant attenuation, y: linear attenuation, z: quadratic attenuation
};

struct SpotLightGPU
{
    vec4 positionRange; // xyz: position, w: range
    vec4 directionInnerCos; // xyz: direction, w: cos(inner angle)
    vec4 attenuationOuterCos; // x: constant attenuation, y: linear attenuation, z: quadratic attenuation, w: cos(outer angle)
    vec4 colorIntensity; // rgb: color, a: intensity
};

layout(std140, binding = 1)uniform LightBlock
{
    ivec4 uLightMeta; // x = directional light count, y = point light count, z = spot light count
    DirectionalLightGPU uDirectionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLightGPU uPointLights[MAX_POINT_LIGHTS];
    SpotLightGPU uSpotLights[MAX_SPOT_LIGHTS];
};