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
    vec4 shadowParams0; // x: enabled, y: technique, z: biasConstant, w: biasSlope
    vec4 shadowParams1; // x: pcfRadiusTexels, y: poissonRadiusUV, z: poissonSamples, w: reserved
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
#define SHADOW_TECHNIQUE_NONE 0u
#define SHADOW_TECHNIQUE_HARD 1u
#define SHADOW_TECHNIQUE_PCF 2u
#define SHADOW_TECHNIQUE_POISSON_PCF 3u
#define SHADOW_TECHNIQUE_PCSS 4u
#define SHADOW_TECHNIQUE_CSM 5u

#define MAX_SHADOW_SAMPLES 32
#define PI 3.141592653589793
#define PI2 6.283185307179586

bool IsDirectionalShadowEnabled(DirectionalLightGPU light)
{
    return light.shadowParams0.x > 0.5;
}

uint GetDirectionalShadowTechnique(DirectionalLightGPU light)
{
    return uint(max(light.shadowParams0.y, 0.0) + 0.5);
}

float GetDirectionalShadowBias(DirectionalLightGPU light, vec3 normal, vec3 lightDir)
{
    // 掠射角(N·L 小)时偏移更大, 减少阴影自遮挡
    float ndotl = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    float biasConstant = max(light.shadowParams0.z, 0.0);
    float biasSlope = max(light.shadowParams0.w, 0.0);
    return max(biasSlope * (1.0 - ndotl), biasConstant);
}

bool ProjectShadowCoords(vec4 fragPosLightSpace, out vec3 projCoords)
{
    // 齐次裁剪空间 -> NDC -> [0,1] 纹理空间
    projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, EPS);
    projCoords = projCoords * 0.5 + 0.5;
    
    // 超出阴影贴图范围直接视为不在阴影
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)return false;
    
    return true;
}

float HardShadow(sampler2D shadowMapSampler, vec3 projCoords, float bias)
{
    float currentDepth = projCoords.z;
    float closestDepth = texture(shadowMapSampler, projCoords.xy).r;
    return (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
}

float PCFShadow(sampler2D shadowMapSampler, vec3 projCoords, float bias, float radiusTexels)
{
    // 3x3 PCF: 邻域深度比较平均, 平滑阴影边缘
    float currentDepth = projCoords.z;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapSampler, 0));
    float shadow = 0.0;
    
    for(int x = -1; x <= 1; ++ x)
    {
        for(int y = -1; y <= 1; ++ y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize * max(radiusTexels, 0.0);
            float closestDepth = texture(shadowMapSampler, projCoords.xy + offset).r;
            shadow += (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
        }
    }
    
    return shadow / 9.0;
}

float GetRandom(vec2 uv)// 伪随机生成采样旋转角
{
    // 0-1
    const highp float a = 12.9898, b = 78.233, c = 43758.5453;
    highp float dt = dot(uv, vec2(a, b)), sn = mod(dt, PI);
    return fract(sin(sn) * c);
}

vec2 Disk[MAX_SHADOW_SAMPLES];
void PoissonDiskSampling(vec2 seed)
{
    // 生成泊松盘近似样本: 不规则分布, 减少规则采样伪影
    float angle = PI2 * GetRandom(seed);
    float radius = 1.0 / float(MAX_SHADOW_SAMPLES);
    float angleStep = 3.883222077450933;
    float radiusStep = radius;
    
    for(int i = 0; i < MAX_SHADOW_SAMPLES; i ++ )
    {
        Disk[i] = vec2(cos(angle), sin(angle)) * pow(radius, 1.0);
        angle += angleStep;
        radius += radiusStep;
    }
}

float PoissonPCFShadow(sampler2D shadowMapSampler, vec3 projCoords, float bias, float radiusUV, int sampleCount)
{
    // 用泊松盘替代网格 PCF, 柔和边缘噪声
    float currentDepth = projCoords.z;
    float shadow = 0.0;
    PoissonDiskSampling(projCoords.xy);
    
    int sampleCountClamped = clamp(sampleCount, 1, MAX_SHADOW_SAMPLES);
    for(int i = 0; i < MAX_SHADOW_SAMPLES; ++ i)
    {
        if (i >= sampleCountClamped)break;
        
        float closestDepth = texture(shadowMapSampler, projCoords.xy + Disk[i] * max(radiusUV, 0.0)).r;
        shadow += (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
    }
    
    return shadow / float(sampleCountClamped);
}

void FindBlockerDepth(sampler2D shadowMapSampler, vec2 uv, float receiverDepth, float bias, vec2 texelSize, float searchRadiusTexels, int blockerSampleCount, out float avgBlockerDepth, out int blockerCount)
{
    // 搜索遮挡者平均深度
    float searchRadiusUV = max(searchRadiusTexels, 0.0) * max(texelSize.x, texelSize.y);
    
    avgBlockerDepth = 0.0;
    blockerCount = 0;
    
    PoissonDiskSampling(uv);
    
    int blockerSampleCountClamped = clamp(blockerSampleCount, 1, MAX_SHADOW_SAMPLES);
    for(int i = 0; i < MAX_SHADOW_SAMPLES; ++ i)
    {
        if (i >= blockerSampleCountClamped)break;
        
        vec2 sampleUV = uv + Disk[i] * searchRadiusUV;
        float sampleDepth = texture(shadowMapSampler, sampleUV).r;
        
        if (sampleDepth < (receiverDepth - bias))
        {
            avgBlockerDepth += sampleDepth;
            blockerCount += 1;
        }
    }
    
    if (blockerCount > 0)avgBlockerDepth /= float(blockerCount);
}

float PCSSDirectional(sampler2D shadowMapSampler, vec3 projCoords, float bias, float blockerSearchTexels, float lightSizeTexels, float minFilterTexels, float maxFilterTexels, int blockerSampleCount, int filterSampleCount)
{
    float receiverDepth = projCoords.z;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapSampler, 0));
    
    float avgBlockerDepth = 0.0;
    int blockerCount = 0;
    FindBlockerDepth(shadowMapSampler, projCoords.xy, receiverDepth, bias, texelSize, blockerSearchTexels, blockerSampleCount, avgBlockerDepth, blockerCount);
    
    // 没找到 blocker 认为不被遮挡
    if (blockerCount == 0)return 0.0;
    
    // 半影估计: receiver 离 blocker 越远, 半影越大
    float penumbraRatio = (receiverDepth - avgBlockerDepth) / max(avgBlockerDepth, EPS);
    float filterRadiusTexels = clamp(penumbraRatio * max(lightSizeTexels, 0.0), max(minFilterTexels, 0.0), max(maxFilterTexels, minFilterTexels));
    float filterRadiusUV = filterRadiusTexels * max(texelSize.x, texelSize.y);
    
    float shadow = 0.0;
    PoissonDiskSampling(projCoords.xy + vec2(0.37, 0.73));
    
    int filterSampleCountClamped = clamp(filterSampleCount, 1, MAX_SHADOW_SAMPLES);
    for(int i = 0; i < MAX_SHADOW_SAMPLES; ++ i)
    {
        if (i >= filterSampleCountClamped)break;
        
        float closestDepth = texture(shadowMapSampler, projCoords.xy + Disk[i] * filterRadiusUV).r;
        shadow += (receiverDepth - bias) > closestDepth ? 1.0 : 0.0;
    }
    
    return shadow / float(filterSampleCountClamped);
}

float EvaluateDirectionalShadow(sampler2D shadowMapSampler, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, DirectionalLightGPU light)
{
    if (!IsDirectionalShadowEnabled(light))return 0.0;
    
    vec3 projCoords;
    if (!ProjectShadowCoords(fragPosLightSpace, projCoords))return 0.0;
    
    float bias = GetDirectionalShadowBias(light, normal, lightDir);
    uint technique = GetDirectionalShadowTechnique(light);
    
    if (technique == SHADOW_TECHNIQUE_HARD)return HardShadow(shadowMapSampler, projCoords, bias);
    
    if (technique == SHADOW_TECHNIQUE_PCF)return PCFShadow(shadowMapSampler, projCoords, bias, light.shadowParams1.x);
    
    if (technique == SHADOW_TECHNIQUE_POISSON_PCF)return PoissonPCFShadow(shadowMapSampler, projCoords, bias, light.shadowParams1.y, int(light.shadowParams1.z + 0.5));
    
    if (technique == SHADOW_TECHNIQUE_PCSS)
    {
        return PCSSDirectional(shadowMapSampler, projCoords, bias, light.shadowParams2.x, light.shadowParams2.y, light.shadowParams2.z, light.shadowParams2.w, int(light.shadowParams3.x + 0.5), int(light.shadowParams3.y + 0.5));
    }
    
    return 0.0;
}