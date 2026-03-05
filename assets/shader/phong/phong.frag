#version 460 core
#extension GL_ARB_shading_language_include : require
out vec4 oPixelColor;

// in
in vec2 oUV;
in vec3 oNormal;
in vec3 oFragPos;

// sampler
uniform sampler2D uAlbedoSampler;

// uniform
uniform vec3 uViewPos;
uniform uint uMaterialFlags;
uniform vec3 uDefaultColor;
uniform float uShininess;

const uint MAT_HAS_ALBEDO_TEX = (1u << 0u);

// light
#include "../common/light.glsl"

void main()
{
    // common
    vec3 N = normalize(oNormal);
    vec3 V = normalize(uViewPos - oFragPos);
    vec3 object_color = ((uMaterialFlags & MAT_HAS_ALBEDO_TEX) != 0u) ? texture(uAlbedoSampler, oUV).rgb : uDefaultColor;
    
    vec3 diffuse_color = vec3(0.0);
    vec3 specular_color = vec3(0.0);
    
    int light_count = clamp(uDirectionalLightCount, 0, MAX_DIRECTIONAL_LIGHTS);
    
    for(int i = 0; i < light_count; ++ i)
    {
        DirectionalLight light = uDirectionalLights[i];
        
        if (light.intensity <= 0.0)
        continue;
        
        vec3 Ld = normalize(light.direction); // 光线方向(世界空间)
        vec3 L = normalize(-Ld);
        vec3 radiance = light.color * light.intensity;
        
        float diffuse = max(dot(N, L), 0.0);
        diffuse_color += radiance * diffuse * object_color;
        
        vec3 R = reflect(-L, N);
        float specular = pow(max(dot(V, R), 0.0), uShininess);
        specular_color += radiance * specular * step(0.0, diffuse);
    }
    
    vec3 ambient_color = vec3(0.08) * object_color;
    
    oPixelColor = vec4(diffuse_color + specular_color + ambient_color, 1.0);
}