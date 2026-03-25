#pragma once
#include "renderpass.hpp"
#include <glad/glad.h>
#include <memory>

namespace core
{
    class DirectionalLight;
    class Shader;

    class ShadowPass final : public IRenderPass
    {
    private:
        std::shared_ptr<Shader> mShadowShader{};
        GLuint mShadowFBO{0};
        GLuint mShadowDepthTexture{0};
        uint32_t mShadowResolution{2048u};
        bool mInitTried{false};

    private:
        /// @brief 初始化阴影贴图资源
        /// @return 是否初始化成功
        bool EnsureInit();

        /// @brief 释放阴影贴图资源
        void Release() noexcept;

        /// @brief 确保阴影贴图资源已初始化
        /// @return 是否初始化成功
        bool EnsureShadowResources();

        /// @brief 选择场景中的方向光光源
        /// @param ctx 帧上下文包含场景、摄像机和渲染队列等信息
        /// @return 方向光光源指针
        const DirectionalLight *SelectDirectionalLight(const FrameContext &ctx) const noexcept;

        /// @brief 聚焦阴影贴图, 动态构建阴影视图投影矩阵(只覆盖真正需要阴影的区域, 所有不透明物体 + 相机视野)
        /// @param ctx 帧上下文包含场景、摄像机和渲染队列等信息
        /// @param light 方向光光源
        /// @return 阴影贴图投影矩阵
        glm::mat4 BuildDirectionalLightVP(const FrameContext &ctx, const DirectionalLight &light) const;

    public:
        ShadowPass() = default;
        ~ShadowPass() override;

        std::string_view GetName() const noexcept override { return "ShadowPass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}