#version 460 core
out vec4 PixelColor;

// in
in vec2 uv;

// uniform
uniform sampler2D texSampler;

void main()
{
    vec4 texColor = texture(texSampler, uv);
    PixelColor = texColor;
}