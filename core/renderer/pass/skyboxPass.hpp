#pragma once
#include "renderpass.hpp"
#include <memory>

namespace core
{
    class Mesh;
    class Shader;

    class SkyboxPass final : public IRenderPass
    {
    private:
        std::shared_ptr<Shader> mShader{};
        std::shared_ptr<Mesh> mSphere{};
        bool mInitTried{false};

        bool EnsureInit();

    public:
        std::string_view GetName() const noexcept override { return "SkyboxPass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}