#version 460 core
const vec2 verts[3] = vec2[3](
    vec2(-1.0, - 1.0),
    vec2(3.0, - 1.0),
    vec2(-1.0, 3.0)
);

out vec2 oUV;

void main()
{
    vec2 pos = verts[gl_VertexID];
    oUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
