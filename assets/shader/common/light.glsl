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
const float specularIntensity = 0.3;

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
    return spec * specularMask * vec3(1.0) * specularIntensity;
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

// shadow
float GetDirectionalShadow(sampler2D shadowMap, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // 从光空间转换到NDC坐标
    vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, EPS);
    projCoords = projCoords * 0.5 + 0.5;
    
    // 如果片段坐标超出阴影贴图的有效范围, 则不在阴影中
    // z > 1.0 表示深度超出远剪裁面, x/y超出[0,1]表示在阴影贴图之外
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)return 0.0;
    
    float currentDepth = projCoords.z;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // 计阴影偏移量, 用于防止自阴影伪影
    // 当法线与光照方向垂直时, 偏移量增大; 平行时, 偏移量减小
    // 避免表面自身遮挡自己造成的阴影错误
    float bias = max(0.0008 * (1.0 - dot(normalize(normal), normalize(lightDir))), 0.00008);
    
    // 如果当前深度减去偏移后大于最近深度说明被遮挡(在阴影中)
    return (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
}