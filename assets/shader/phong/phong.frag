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
    
    // directional light
    int light_count = clamp(uLightMeta.x, 0, MAX_DIRECTIONAL_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        DirectionalLightGPU light = uDirectionalLights[i];
        vec3 diffuse, specular;
        GetDirectionalLight(light, N, V, object_color, uShininess, specular_mask, diffuse, specular);
        diffuse_color += diffuse;
        specular_color += specular;
    }
    
    // point light
    light_count = clamp(uLightMeta.y, 0, MAX_POINT_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        PointLightGPU light = uPointLights[i];
        vec3 diffuse, specular;
        GetPointLight(light, oFragPos, N, V, object_color, uShininess, specular_mask, diffuse, specular);
        diffuse_color += diffuse;
        specular_color += specular;
    }
    
    // spot light
    light_count = clamp(uLightMeta.z, 0, MAX_SPOT_LIGHTS);
    for(int i = 0; i < light_count; ++ i)
    {
        SpotLightGPU light = uSpotLights[i];
        vec3 diffuse, specular;
        GetSpotLight(light, oFragPos, N, V, object_color, uShininess, specular_mask, diffuse, specular);
        diffuse_color += diffuse;
        specular_color += specular;
    }
    
    vec3 ambient_color = vec3(0.08) * object_color;
    
    oPixelColor = vec4(diffuse_color + specular_color + ambient_color, 1.0);
}