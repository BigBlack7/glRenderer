#pragma once
#include "renderer/renderpass.hpp"
#include <memory>

namespace core
{
    class Shader;

    class PostProcessPass final : public IRenderPass
    {
    private:
        std::shared_ptr<Shader> mShader{};
        bool mInitTried{false};

        bool EnsureInit();

    public:
        std::string_view GetName() const noexcept override { return "PostProcessPass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}