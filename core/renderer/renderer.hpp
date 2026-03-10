#pragma once
#include "camera/camera.hpp"
#include "renderBackend.hpp"
#include "rendergraph.hpp"
#include "profiler.hpp"
#include "scene/scene.hpp"
#include <glm/glm.hpp>

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

    private:
        /// @brief 
        /// @return 
        bool EnsureInit();

    public:
        Renderer() = default;
        ~Renderer() = default;

        /// @brief 
        void Init();

        /// @brief 
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

        /// @brief 
        /// @param scene 
        /// @param camera 
        /// @param timeSec 
        void Render(Scene &scene, const Camera &camera, float timeSec = 0.f);
    };
}