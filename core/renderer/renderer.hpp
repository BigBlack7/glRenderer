#pragma once
#include "camera/camera.hpp"
#include "renderBackend.hpp"
#include "rendergraph.hpp"
#include "buffer/frameBuffer.hpp"
#include "profiler.hpp"
#include "scene/scene.hpp"
#include <glm/glm.hpp>
#include <cstdint>

namespace core
{
    class Renderer final
    {
    private:
        glm::vec4 mClearColor{0.68f, 0.85f, 0.90f, 1.f}; // 默认天空蓝清屏颜色
        RenderBackend mBackend{};
        RenderGraph mGraph{};
        RenderProfiler mInfos{};
        bool mInitialized{false};

        FrameBuffer mSceneColorTarget{};
        uint32_t mTargetWidth{0};
        uint32_t mTargetHeight{0};

    private:
        /// @brief 确保渲染器已初始化
        /// @return 是否成功初始化
        bool EnsureInit();

        /// @brief 确保场景颜色目标已创建
        /// @return 是否成功创建场景颜色目标
        bool EnsureSceneTarget();

    public:
        Renderer() = default;
        ~Renderer() = default;

        /// @brief 初始化渲染器, 添加渲染pass
        void Init();

        /// @brief 关闭渲染器, 释放所有资源
        void Shutdown();

        void SetClearColor(const glm::vec4 &clearColor)
        {
            mClearColor = clearColor;
            if (mInitialized)
            {
                mBackend.SetClearColor(mClearColor); // 实时更新
            }
        }

        const RenderProfiler &GetStats() const noexcept { return mInfos; }

        /// @brief 执行渲染流程
        /// @param scene 要渲染的场景  
        /// @param camera 要渲染的相机
        /// @param timeSec 渲染时间, 单位秒
        void Render(Scene &scene, const Camera &camera, float timeSec = 0.f);
    };
}