#version 460 core

in vec3 oFragPos;

uniform vec3 uLightPos;
uniform float uFarPlane;

void main()
{
    float edge = length(oFragPos - uLightPos) / (1.414 * uFarPlane);
    gl_FragDepth = edge;
}