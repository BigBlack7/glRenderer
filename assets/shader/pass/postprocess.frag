#version 460 core

in vec2 oUV;
out vec4 oPixelColor;

uniform sampler2D uSceneSampler;
uniform sampler2D uBloomSampler;
uniform uint uGammaEnabled = 1u;
uniform uint uToneMapMode = 1u; // 0: Reinhard, 1: Exposure
uniform uint uBloomEnabled = 1u;
uniform float uBloomIntensity = 0.08;
uniform float uGamma = 2.2;
uniform float uExposure = 1.0;
uniform float uSaturation = 1.0;
uniform float uContrast = 1.0;
uniform float uVignette = 0.0;

vec3 ToneMapReinhard(vec3 hdr)
{
    return hdr / (hdr + vec3(1.0));
}

vec3 ToneMapExposure(vec3 hdr, float exposure)
{
    return vec3(1.0) - exp(-hdr * max(exposure, 0.0));
}

void main()
{
    vec3 hdrColor = max(texture(uSceneSampler, oUV).rgb, vec3(0.0));
    if (uBloomEnabled != 0u) hdrColor += max(texture(uBloomSampler, oUV).rgb, vec3(0.0)) * max(uBloomIntensity, 0.0);
    
    vec3 color = (uToneMapMode == 0u) ? ToneMapReinhard(hdrColor) : ToneMapExposure(hdrColor, uExposure);
    
    // Saturation
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, max(uSaturation, 0.0));
    
    // Contrast around mid-gray
    color = (color - vec3(0.5)) * max(uContrast, 0.0) + vec3(0.5);
    
    // Vignette
    vec2 centered = oUV * 2.0 - 1.0;
    float r = length(centered);
    float vig = mix(1.0, 1.0 - smoothstep(0.2, 1.0, r), clamp(uVignette, 0.0, 1.0));
    color *= vig;
    
    // Gamma
    if (uGammaEnabled != 0u)
    {
        float invGamma = 1.0 / max(uGamma, 0.001);
        color = pow(max(color, vec3(0.0)), vec3(invGamma));
    }
    
    oPixelColor = vec4(color, 1.0);
}