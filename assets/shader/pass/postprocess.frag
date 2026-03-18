#version 460 core

in vec2 oUV;
out vec4 oPixelColor;

uniform sampler2D uSceneSampler;

void main()
{
    vec3 color = texture(uSceneSampler, oUV).rgb;
    oPixelColor = vec4(color, 1.0);
}