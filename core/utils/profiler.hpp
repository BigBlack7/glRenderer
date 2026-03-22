#pragma once
#include <cstdint>

namespace core
{
    // 记录每帧的渲染性能数据, 用于性能分析和调试
    struct RenderProfiler final
    {
        uint32_t __drawCalls__{0};    // 绘制调用次数(glDraw*调用)
        uint32_t __triangles__{0};    // 渲染的三角形总数
        uint32_t __programBinds__{0}; // 着色器程序绑定次数
        uint32_t __textureBinds__{0}; // 纹理绑定次数
    };
}