#pragma once
#include "renderpass.hpp"
#include <memory>

namespace core
{
    class Shader;
    class Mesh;

    class LightProxyPass final : public IRenderPass
    {
    private:
        std::shared_ptr<Shader> mProxyShader{};
        std::shared_ptr<Mesh> mSphere{};
        bool mInitTried{false};

    private:
        bool EnsureInit();

    public:
        LightProxyPass() = default;
        ~LightProxyPass() override = default;

        std::string_view GetName() const noexcept override { return "LightProxyPass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}
