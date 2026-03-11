#version 460 core
#extension GL_ARB_shading_language_include : require
out vec4 oPixelColor;

// in
in vec2 oUV;
in vec3 oNormal;
in vec3 oFragPos;

// sampler
uniform sampler2D uAlbedoSampler;
uniform sampler2D uSpecularMaskSampler;

// uniform
uniform uint uMaterialFlags;
uniform vec3 uDefaultColor;
uniform float uShininess;

const uint MAT_HAS_ALBEDO_TEX = (1u << 0u); // 漫反射贴图
const uint MAT_HAS_SPECULAR_MASK_TEX = (1u << 6u); // 高光遮罩贴图

layout(std140, binding = 0)uniform FrameBlock
{
    mat4 uV;
    mat4 uP;
    vec4 uViewPosTime;
};

#include "../common/light.glsl"

void main()
{
    // common
    vec3 N = normalize(oNormal);
    vec3 V = normalize(uViewPosTime.xyz - oFragPos);
    vec3 object_color = ((uMaterialFlags & MAT_HAS_ALBEDO_TEX) != 0u) ? texture(uAlbedoSampler, oUV).rgb : uDefaultColor;
    float specular_mask = ((uMaterialFlags & MAT_HAS_SPECULAR_MASK_TEX) != 0u) ? texture(uSpecularMaskSampler, oUV).r : 1.0;
    
    vec3 diffuse_color = vec3(0.0);
    vec3 specular_color = vec3(0.0);
    
    /*
    TODO
    当片元刚好在光源位置时，normalize(0)。
    衰减分母可能接近 0。
    聚光内外锥分母可能接近 0。
    修改点：加入 epsilon 防护，先算距离再归一化，分母用 max(denom, EPS)。
    */
    
    // directional light
    int light_count = clamp(uLightMeta.x, 0, MAX_DIRECTIONAL_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        DirectionalLightGPU light = uDirectionalLights[i];
        float intensity = light.colorIntensity.a;
        if (intensity <= 0.0)continue;
        
        vec3 L = normalize(-light.direction.xyz); // 光线方向(世界空间)
        vec3 radiance = light.colorIntensity.rgb * intensity;
        
        float diffuse = max(dot(N, L), 0.0);
        diffuse_color += radiance * diffuse * object_color;
        
        vec3 H = normalize(L + V);
        float specular = pow(max(dot(N, H), 0.0), uShininess);
        specular_color += radiance * specular * diffuse * specular_mask;
    }
    
    // point light
    light_count = clamp(uLightMeta.y, 0, MAX_POINT_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        PointLightGPU light = uPointLights[i];
        float intensity = light.colorIntensity.a;
        if (intensity <= 0.0)continue;
        
        vec3 L = normalize(light.positionRange.xyz - oFragPos); // 光线方向(世界空间)
        float D = length(light.positionRange.xyz - oFragPos); // 光线距离(世界空间)
        if (D > light.positionRange.w)continue; // 超出范围不受影响
        float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * D + light.attenuation.z * D * D);
        vec3 radiance = light.colorIntensity.rgb * intensity * attenuation;
        
        float diffuse = max(dot(N, L), 0.0);
        diffuse_color += radiance * diffuse * object_color;
        
        vec3 H = normalize(L + V);
        float specular = pow(max(dot(N, H), 0.0), uShininess);
        specular_color += radiance * specular * diffuse * specular_mask;
    }
    
    // spot light
    light_count = clamp(uLightMeta.z, 0, MAX_SPOT_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        SpotLightGPU light = uSpotLights[i];
        float intensity = light.colorIntensity.a;
        if (intensity <= 0.0)continue;
        
        vec3 L = normalize(light.positionRange.xyz - oFragPos); // 光线方向(世界空间)
        float D = length(light.positionRange.xyz - oFragPos); // 光线距离(世界空间)
        if (D > light.positionRange.w)continue; // 超出范围不受影响
        
        float attenuation = 1.0 / (light.attenuationOuterCos.x + light.attenuationOuterCos.y * D + light.attenuationOuterCos.z * D * D);
        vec3 target = normalize(-light.directionInnerCos.xyz); // 聚光方向(世界空间)
        float theta = dot(L, target);
        float spot_effect = clamp((theta - light.attenuationOuterCos.w) / (light.directionInnerCos.w - light.attenuationOuterCos.w), 0.0, 1.0);
        
        vec3 radiance = light.colorIntensity.rgb * intensity * attenuation * spot_effect;
        
        float diffuse = max(dot(N, L), 0.0);
        diffuse_color += radiance * diffuse * object_color;
        
        vec3 H = normalize(L + V);
        float specular = pow(max(dot(N, H), 0.0), uShininess);
        specular_color += radiance * specular * diffuse * specular_mask;
    }
    
    vec3 ambient_color = vec3(0.08) * object_color;
    
    oPixelColor = vec4(diffuse_color + specular_color + ambient_color, 1.0);
}