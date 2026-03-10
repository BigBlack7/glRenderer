#pragma once
#include "camera/camera.hpp"
#include "renderQueue.hpp"
#include "profiler.hpp"
#include "scene/scene.hpp"
#include <glm/glm.hpp>

namespace core
{
    /*
     * FrameContext为渲染通道间的共享数据结构:
     * - 包含场景、摄像机等基础数据
     * - 提供渲染队列用于收集可渲染对象
     * - 包含性能统计信息
     * - 可扩展为包含更多渲染数据
     */
    struct FrameContext final
    {
        Scene *__scene__{nullptr};
        const Camera *__camera__{nullptr};
        float __timeSec__{0.f};

        RenderQueue __opaqueQueue__{};
        RenderProfiler __stats__{};
    };
}