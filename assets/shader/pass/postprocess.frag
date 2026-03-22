#version 460 core

in vec2 oUV;
out vec4 oPixelColor;

uniform sampler2D uSceneSampler;

const float invGamma = 1.0 / 2.2;

void main()
{
    vec3 color = texture(uSceneSampler, oUV).rgb;
    oPixelColor = vec4(pow(color, vec3(invGamma)), 1.0);
}