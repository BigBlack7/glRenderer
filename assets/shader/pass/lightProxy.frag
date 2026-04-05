#version 460 core
out vec4 oPixelColor;

uniform vec3 uColor;

void main()
{
    oPixelColor = vec4(uColor, 1.0);
}
