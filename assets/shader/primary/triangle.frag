#version 460 core
out vec4 PixelColor;

// in
in vec3 color;

// uniform
uniform float time;
uniform vec3 icolor;

void main()
{
    PixelColor = vec4(icolor * (0.5 + 0.5 * sin(time)), 1.0);
}