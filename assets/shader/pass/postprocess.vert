#version 460 core

out vec2 oUV;

const vec2 Positions[3] = vec2[]
(
    vec2(-1.0, - 1.0),
    vec2(3.0, - 1.0),
    vec2(-1.0, 3.0)
);

void main()
{
    vec2 pos = Positions[gl_VertexID];
    oUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}