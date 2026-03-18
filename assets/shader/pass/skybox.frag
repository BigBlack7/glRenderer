#version 460 core
#extension GL_ARB_shading_language_include : require
in vec3 oUVW;
out vec4 oPixelColor;

uniform samplerCube uSkyboxCube;
uniform sampler2D uSkyboxPanorama;
uniform int uSkyMode; // 0: cubemap, 1: panorama
uniform float uSkyboxIntensity = 1.0;

#include "../common/skybox.glsl"

void main()
{
    vec3 dir = normalize(oUVW);
    vec3 color = vec3(0.0);
    
    if (uSkyMode == 0)color = texture(uSkyboxCube, dir).rgb;
    else color = texture(uSkyboxPanorama, UvwToUv(dir)).rgb;
    
    oPixelColor = vec4(color * uSkyboxIntensity, 1.0);
}