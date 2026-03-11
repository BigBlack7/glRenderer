#ifndef MAX_DIRECTIONAL_LIGHTS
#define MAX_DIRECTIONAL_LIGHTS 4
#endif

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 16
#endif

#ifndef MAX_SPOT_LIGHTS
#define MAX_SPOT_LIGHTS 8
#endif

const float EPS = 0.000001;

struct DirectionalLightGPU
{
    vec4 direction; // xyz: direction
    vec4 colorIntensity; // rgb: color, a: intensity
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

// ===================================光照=====================================

vec3 GetDiffuse(vec3 normal, vec3 lightDir)
{
    return max(dot(normal, lightDir), 0.0) * vec3(1.0);
}

vec3 GetSpecular(vec3 normal, vec3 viewDir, vec3 lightDir, float shininess, float specularMask)
{
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    return spec * specularMask * vec3(1.0);
}

vec3 GetDirectionalLight(DirectionalLightGPU light, vec3 normal, vec3 viewDir, vec3 objectColor, float shininess, float specularMask, out vec3 diffuseOut, out vec3 specularOut)
{
    float intensity = light.colorIntensity.a;
    if (intensity <= 0.0)
    {
        diffuseOut = vec3(0.0);
        specularOut = vec3(0.0);
        return vec3(0.0);
    }
    
    vec3 lightDir = normalize(-light.direction.xyz);
    vec3 radiance = light.colorIntensity.rgb * intensity;
    
    diffuseOut = radiance * GetDiffuse(normal, lightDir) * objectColor;
    specularOut = radiance * GetSpecular(normal, viewDir, lightDir, shininess, specularMask) * max(dot(normal, lightDir), 0.0);
    
    return radiance;
}

vec3 GetPointLight(PointLightGPU light, vec3 fragPos, vec3 normal, vec3 viewDir, vec3 objectColor, float shininess, float specularMask, out vec3 diffuseOut, out vec3 specularOut)
{
    float intensity = light.colorIntensity.a;
    if (intensity <= 0.0)
    {
        diffuseOut = vec3(0.0);
        specularOut = vec3(0.0);
        return vec3(0.0);
    }
    
    vec3 lightVec = light.positionRange.xyz - fragPos;
    float distance = length(lightVec);
    
    if (distance > light.positionRange.w)
    {
        diffuseOut = vec3(0.0);
        specularOut = vec3(0.0);
        return vec3(0.0);
    }
    
    vec3 lightDir = normalize(lightVec);
    float attenuation = 1.0 / max(light.attenuation.x + light.attenuation.y * distance +
    light.attenuation.z * distance * distance, EPS);
    vec3 radiance = light.colorIntensity.rgb * intensity * attenuation;
    
    diffuseOut = radiance * GetDiffuse(normal, lightDir) * objectColor;
    specularOut = radiance * GetSpecular(normal, viewDir, lightDir, shininess, specularMask) * max(dot(normal, lightDir), 0.0);
    
    return radiance;
}

vec3 GetSpotLight(SpotLightGPU light, vec3 fragPos, vec3 normal, vec3 viewDir, vec3 objectColor, float shininess, float specularMask, out vec3 diffuseOut, out vec3 specularOut)
{
    float intensity = light.colorIntensity.a;
    if (intensity <= 0.0)
    {
        diffuseOut = vec3(0.0);
        specularOut = vec3(0.0);
        return vec3(0.0);
    }
    
    vec3 lightVec = light.positionRange.xyz - fragPos;
    float distance = length(lightVec);
    
    if (distance > light.positionRange.w)
    {
        diffuseOut = vec3(0.0);
        specularOut = vec3(0.0);
        return vec3(0.0);
    }
    
    vec3 lightDir = normalize(lightVec);
    vec3 spotDir = normalize(-light.directionInnerCos.xyz);
    
    float theta = dot(lightDir, spotDir);
    float spotEffect = clamp((theta - light.attenuationOuterCos.w) / max(light.directionInnerCos.w - light.attenuationOuterCos.w, EPS), 0.0, 1.0);
    
    float attenuation = 1.0 / max(light.attenuationOuterCos.x + light.attenuationOuterCos.y * distance + light.attenuationOuterCos.z * distance * distance, EPS);
    vec3 radiance = light.colorIntensity.rgb * intensity * attenuation * spotEffect;
    
    diffuseOut = radiance * GetDiffuse(normal, lightDir) * objectColor;
    specularOut = radiance * GetSpecular(normal, viewDir, lightDir, shininess, specularMask) * max(dot(normal, lightDir), 0.0);
    
    return radiance;
}