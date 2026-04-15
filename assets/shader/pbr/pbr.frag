#version 460 core
#extension GL_ARB_shading_language_include : require
out vec4 oPixelColor;

in vec2 oUV;
in vec3 oNormal;
in vec3 oFragPos;
in vec3 oTangent;
in vec4 oFragPosLightSpace;

uniform sampler2D uAlbedoSampler;
uniform sampler2D uNormalSampler;
uniform sampler2D uMetallicRoughSampler;

uniform uint uMaterialFlags;
uniform vec3 uBaseColor = vec3(1.0, 1.0, 1.0);
uniform float uMetallic = 0.0;
uniform float uRoughness = 0.6;
uniform float uNormalStrength = 1.0;

const uint MAT_HAS_ALBEDO_TEX = (1u << 0u);
const uint MAT_HAS_NORMAL_TEX = (1u << 1u);
const uint MAT_HAS_METALLIC_ROUGHNESS_TEX = (1u << 2u);

layout(std140, binding = 0)uniform FrameBlock
{
    mat4 uV;
    mat4 uP;
    vec4 uViewPosTime;
};

#include "../common/light.glsl"
#include "../common/pbr.glsl"
#include "../common/shadow.glsl"

uniform sampler2D uShadowMapSampler;
uniform sampler2DArray uShadowMapArraySampler;
uniform samplerCube uPointShadowCubeSampler;
uniform sampler2D uSpotShadowMapSampler;
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D uBrdfLut;
uniform uint uHasDirectionalShadow = 0u;
uniform uint uHasDirectionalCSM = 0u;
uniform uint uHasPointShadow = 0u;
uniform uint uHasSpotShadow = 0u;
uniform uint uDirectionalCascadeCount = 0u;
uniform float uDirectionalCascadeSplits[4];
uniform float uDirectionalCascadeUVScales[4];
uniform mat4 uDirectionalLightVPArray[4];
uniform vec3 uPointShadowLightPos = vec3(0.0);
uniform float uPointShadowFarPlane = 1.0;
uniform float uPointShadowBiasConstant = 0.0008;
uniform float uPointShadowBiasSlope = 0.002;
uniform mat4 uSpotLightVP = mat4(1.0);
uniform float uSpotShadowBiasConstant = 0.0008;
uniform float uSpotShadowBiasSlope = 0.002;

void main()
{
    vec3 V = normalize(uViewPosTime.xyz - oFragPos);
    
    vec3 albedo = ((uMaterialFlags & MAT_HAS_ALBEDO_TEX) != 0u)
    ? texture(uAlbedoSampler, oUV).rgb
    : uBaseColor;
    
    vec3 N = normalize(oNormal);
    if ((uMaterialFlags & MAT_HAS_NORMAL_TEX) != 0u)
    {
        vec3 T = normalize(oTangent);
        
        // Gram-Schmidt 正交化
        T = normalize(T - N * dot(T, N));
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);
        
        vec3 nTex = texture(uNormalSampler, oUV).rgb * 2.0 - vec3(1.0);
        vec3 mapped = normalize(TBN * nTex);
        N = normalize(mix(N, mapped, clamp(uNormalStrength, 0.0, 1.0)));
    }
    
    float metallic = clamp(uMetallic, 0.0, 1.0);
    float roughness = clamp(uRoughness, 0.04, 1.0);
    if ((uMaterialFlags & MAT_HAS_METALLIC_ROUGHNESS_TEX) != 0u)
    {
        vec3 mr = texture(uMetallicRoughSampler, oUV).rgb;
        roughness = clamp(mr.g, 0.04, 1.0);
        metallic = clamp(mr.b, 0.0, 1.0);
    }
    
    vec3 color = vec3(0.0);
    
    int lightCount = clamp(uLightMeta.x, 0, MAX_DIRECTIONAL_LIGHTS);
    for(int i = 0; i < lightCount; ++ i)
    {
        DirectionalLightGPU light = uDirectionalLights[i];
        vec3 L = normalize(-light.direction.xyz);
        vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.a;
        float viewDepth = -(uV * vec4(oFragPos, 1.0)).z;
        float shadow = (uHasDirectionalShadow != 0u)
        ? EvaluateDirectionalShadow(uShadowMapSampler,
            uShadowMapArraySampler,
            oFragPosLightSpace,
            oFragPos,
            viewDepth,
            N,
            L,
            light,
            uHasDirectionalCSM,
            uDirectionalCascadeCount,
            uDirectionalCascadeSplits,
            uDirectionalCascadeUVScales,
        uDirectionalLightVPArray)
        : 0.0;
        
        color += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness) * (1.0 - shadow);
    }
    
    lightCount = clamp(uLightMeta.y, 0, MAX_POINT_LIGHTS);
    for(int i = 0; i < lightCount; ++ i)
    {
        PointLightGPU light = uPointLights[i];
        vec3 toLight = light.positionRange.xyz - oFragPos;
        float distanceToLight = length(toLight);
        if (distanceToLight > light.positionRange.w)
        continue;
        
        vec3 L = normalize(toLight);
        float attenuation = 1.0 / max(light.attenuation.x + light.attenuation.y * distanceToLight + light.attenuation.z * distanceToLight * distanceToLight, EPS);
        vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.a * attenuation;
        
        float shadow = 0.0;
        if (uHasPointShadow != 0u && i == 0)
        {
            shadow = EvaluatePointShadow(uPointShadowCubeSampler,
                oFragPos,
                N,
                uPointShadowLightPos,
                uPointShadowFarPlane,
                uPointShadowBiasConstant,
            uPointShadowBiasSlope);
        }
        
        color += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness) * (1.0 - shadow);
    }
    
    lightCount = clamp(uLightMeta.z, 0, MAX_SPOT_LIGHTS);
    for(int i = 0; i < lightCount; ++ i)
    {
        SpotLightGPU light = uSpotLights[i];
        vec3 toLight = light.positionRange.xyz - oFragPos;
        float distanceToLight = length(toLight);
        if (distanceToLight > light.positionRange.w)
        continue;
        
        vec3 L = normalize(toLight);
        vec3 spotDir = normalize(-light.directionInnerCos.xyz);
        float theta = dot(L, spotDir);
        float spot = clamp((theta - light.attenuationOuterCos.w) / max(light.directionInnerCos.w - light.attenuationOuterCos.w, EPS), 0.0, 1.0);
        if (spot <= 0.0)
        continue;
        
        float attenuation = 1.0 / max(light.attenuationOuterCos.x + light.attenuationOuterCos.y * distanceToLight + light.attenuationOuterCos.z * distanceToLight * distanceToLight, EPS);
        vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.a * attenuation * spot;
        
        float shadow = 0.0;
        if (uHasSpotShadow != 0u && i == 0)
        {
            shadow = EvaluateSpotShadow(uSpotShadowMapSampler,
                oFragPos,
                N,
                L,
                uSpotLightVP,
                uSpotShadowBiasConstant,
            uSpotShadowBiasSlope);
        }
        
        color += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness) * (1.0 - shadow);
    }
    // TODO Energy Compensation
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    
    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;
    
    vec3 R = reflect(-V, N);
    const float maxMip = float(max(textureQueryLevels(uPrefilterMap) - 1, 0));
    vec3 prefilteredColor = textureLod(uPrefilterMap, R, roughness * maxMip).rgb;
    vec2 brdf = texture(uBrdfLut, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
    
    vec3 ambient = kD * diffuseIBL + specularIBL;
    oPixelColor = vec4(color + ambient, 1.0);
}