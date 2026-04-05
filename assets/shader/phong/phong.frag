#version 460 core
#extension GL_ARB_shading_language_include : require
out vec4 oPixelColor;

// in
in vec2 oUV;
in vec3 oNormal;
in vec3 oFragPos;
in mat3 oTBN;
in vec4 oFragPosLightSpace;

// sampler
uniform sampler2D uAlbedoSampler;
uniform sampler2D uNormalSampler;
uniform sampler2D uMetallicRoughSampler;
uniform sampler2D uAOSampler;
uniform sampler2D uOpacitySampler;
uniform sampler2D uHeightSampler;

// uniform
uniform uint uMaterialFlags;
uniform vec3 uBaseColor = vec3(1.0);
uniform float uShininess = 64.0;
uniform uint uAlphaMode = 0u; // 0 Opaque, 1 Cutout, 2 Transparent
uniform float uAlphaCutoff = 0.5;
uniform float uOpacity = 1.0;

const uint MAT_HAS_ALBEDO_TEX = (1u << 0u); // 漫反射贴图
const uint MAT_HAS_NORMAL_TEX = (1u << 1u); // 法线向量贴图
const uint MAT_HAS_METALLIC_ROUGHNESS_TEX = (1u << 2u); // 金属粗糙度贴图
const uint MAT_HAS_AO_TEX = (1u << 3u); // 环境光遮蔽贴图
const uint MAT_HAS_OPACITY_TEX = (1u << 5u); // 不透明度贴图
const uint MAT_HAS_HEIGHT_TEX = (1u << 6u); // 高度/位移贴图

layout(std140, binding = 0)uniform FrameBlock
{
    mat4 uV;
    mat4 uP;
    vec4 uViewPosTime;
};

#include "../common/light.glsl"

// shadow
uniform sampler2D uShadowMapSampler;
uniform sampler2DArray uShadowMapArraySampler;
uniform samplerCube uPointShadowCubeSampler;
uniform sampler2D uSpotShadowMapSampler;
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

vec2 SteepParallaxUV(vec2 uv, vec3 V)
{
    V = normalize(transpose(oTBN) * V); // 世界空间 坐标转换到uv坐标的切线空间坐标
    float layer_num = 20.0;
    float layer_depth = 1.0 / layer_num;
    vec2 deltaUV = V.xy / V.z * 0.1 / layer_num;
    
    vec2 current_uv = uv;
    float current_sample_val = texture(uHeightSampler, current_uv).r;
    float current_layer_depth = 0.0;
    
    while(current_sample_val > current_layer_depth)
    {
        current_uv -= deltaUV;
        current_sample_val = texture(uHeightSampler, current_uv).r;
        current_layer_depth += layer_depth;
    }
    
    vec2 last_uv = current_uv + deltaUV;
    float last_layer_depth = current_layer_depth - layer_depth;
    float last_height = texture(uHeightSampler, last_uv).r;
    float last_length = last_height - last_layer_depth;
    float current_length = current_layer_depth - current_sample_val;
    float last_weight = current_length / (last_length + current_length);
    vec2 target_uv = mix(current_uv, last_uv, last_weight);
    
    return target_uv;
}

void main()
{
    // common
    vec3 V = normalize(uViewPosTime.xyz - oFragPos);
    vec2 uv = ((uMaterialFlags & MAT_HAS_HEIGHT_TEX) != 0u) ? SteepParallaxUV(oUV, V) : oUV;
    
    // material features
    vec3 object_color = ((uMaterialFlags & MAT_HAS_ALBEDO_TEX) != 0u) ? texture(uAlbedoSampler, uv).rgb : uBaseColor;
    vec3 N = ((uMaterialFlags & MAT_HAS_NORMAL_TEX) != 0u) ? normalize(oTBN * ((texture(uNormalSampler, uv).rgb) * 2.0 - vec3(1.0))) : normalize(oNormal);
    
    float base_alpha = ((uMaterialFlags & MAT_HAS_ALBEDO_TEX) != 0u) ? texture(uAlbedoSampler, uv).a : 1.0;
    float opacity_mask = ((uMaterialFlags & MAT_HAS_OPACITY_TEX) != 0u) ? texture(uOpacitySampler, uv).r : 1.0;
    float alpha = clamp(base_alpha * opacity_mask * uOpacity, 0.0, 1.0);
    
    if (uAlphaMode == 1u && alpha < uAlphaCutoff)discard;
    if (uAlphaMode == 0u)alpha = 1.0;
    
    float specular_mask = ((uMaterialFlags & MAT_HAS_METALLIC_ROUGHNESS_TEX) != 0u) ? texture(uMetallicRoughSampler, uv).r : 1.0;
    float ao_mask = ((uMaterialFlags & MAT_HAS_AO_TEX) != 0u) ? texture(uAOSampler, uv).r : 1.0;
    
    vec3 diffuse_color = vec3(0.0);
    vec3 specular_color = vec3(0.0);
    
    // directional light
    int light_count = clamp(uLightMeta.x, 0, MAX_DIRECTIONAL_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        DirectionalLightGPU light = uDirectionalLights[i];
        vec3 diffuse, specular;
        GetDirectionalLight(light, N, V, object_color, uShininess, specular_mask, diffuse, specular);
        float viewDepth = -(uV * vec4(oFragPos, 1.0)).z;
        float shadow = (uHasDirectionalShadow != 0u) ? EvaluateDirectionalShadow(uShadowMapSampler,
            uShadowMapArraySampler,
            oFragPosLightSpace,
            oFragPos,
            viewDepth,
            N,
            normalize(-light.direction.xyz),
            light,
            uHasDirectionalCSM,
            uDirectionalCascadeCount,
            uDirectionalCascadeSplits,
            uDirectionalCascadeUVScales,
        uDirectionalLightVPArray) : 0.0;
        
        diffuse_color += diffuse * (1.0 - shadow);
        specular_color += specular * (1.0 - shadow);
    }
    
    // point light
    light_count = clamp(uLightMeta.y, 0, MAX_POINT_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        PointLightGPU light = uPointLights[i];
        vec3 diffuse, specular;
        GetPointLight(light, oFragPos, N, V, object_color, uShininess, specular_mask, diffuse, specular);
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
        diffuse_color += diffuse * (1.0 - shadow);
        specular_color += specular * (1.0 - shadow);
    }
    
    // spot light
    light_count = clamp(uLightMeta.z, 0, MAX_SPOT_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        SpotLightGPU light = uSpotLights[i];
        vec3 diffuse, specular;
        GetSpotLight(light, oFragPos, N, V, object_color, uShininess, specular_mask, diffuse, specular);
        float shadow = 0.0;
        if (uHasSpotShadow != 0u && i == 0)
        {
            shadow = EvaluateSpotShadow(uSpotShadowMapSampler,
                oFragPos,
                N,
                normalize(light.positionRange.xyz - oFragPos),
                uSpotLightVP,
                uSpotShadowBiasConstant,
            uSpotShadowBiasSlope);
        }
        diffuse_color += diffuse * (1.0 - shadow);
        specular_color += specular * (1.0 - shadow);
    }
    
    vec3 ambient_color = vec3(0.1) * object_color * ao_mask;
    
    oPixelColor = vec4(diffuse_color + specular_color + ambient_color, alpha);
}