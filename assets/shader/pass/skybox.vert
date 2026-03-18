#version 460 core
layout(location = 0)in vec3 lPosition;

out vec3 oUVW;

layout(std140, binding = 0)uniform FrameBlock
{
    mat4 uV;
    mat4 uP;
    vec4 uViewPosTime;
};

void main()
{
    oUVW = lPosition; // 直接用顶点坐标作为3D纹理采样坐标
    
    // 移除视图矩阵的平移分量, 只保留旋转使天空盒中心对准相机
    mat4 viewNoTranslation = mat4(mat3(uV));
    vec4 clip = uP * viewNoTranslation * vec4(lPosition, 1.0);
    
    // 深度固定到远平面, 配合LEQUAL只填背景像素
    gl_Position = clip.xyww;
}