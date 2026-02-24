#version 460 core
out vec4 PixelColor;

// in
in vec3 color;
in vec2 uv;

// uniform
uniform sampler2D grassSampler;
uniform sampler2D landSampler;
uniform sampler2D noiseSampler;

void main()
{
    vec4 grassColor = texture(grassSampler, uv);
    vec4 landColor = texture(landSampler, uv);
    float weight = texture(noiseSampler, uv).r;
    PixelColor = vec4(mix(grassColor.rgb, landColor.rgb, weight), 1.0);
}