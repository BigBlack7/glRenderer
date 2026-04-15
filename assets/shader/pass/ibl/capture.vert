#version 460 core
layout(location = 0)in vec3 lPosition;

out vec3 oLocalDir;

uniform mat4 uProjection;
uniform mat4 uView;

void main()
{
    oLocalDir = lPosition;
    gl_Position = uProjection * uView * vec4(lPosition, 1.0);
}
