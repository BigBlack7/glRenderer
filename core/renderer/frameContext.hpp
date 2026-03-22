#pragma once
#include "camera/camera.hpp"
#include "renderQueue.hpp"
#include "utils/profiler.hpp"
#include "scene/scene.hpp"
#include <glm/glm.hpp>
#include <cstdint>

namespace core
{
    class FrameBuffer;
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

        // 渲染目标FBO
        FrameBuffer *__sceneColorTarget__{nullptr};
        uint32_t __targetWidth__{0};
        uint32_t __targetHeight__{0};

        RenderQueue __renderQueue__{};
        RenderProfiler __stats__{};
    };
}