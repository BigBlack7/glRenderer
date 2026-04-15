#version 460 core
#extension GL_ARB_shading_language_include : require
in vec3 oUVW;
out vec4 oPixelColor;

uniform samplerCube uSkyboxCube;
uniform float uSkyboxIntensity = 1.0;

void main()
{
    vec3 dir = normalize(oUVW);
    vec3 color = texture(uSkyboxCube, dir).rgb;
    oPixelColor = vec4(color * uSkyboxIntensity, 1.0);
}