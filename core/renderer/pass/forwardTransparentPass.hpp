#pragma once
#include "renderer/renderpass.hpp"

namespace core
{
    class ForwardTransparentPass final : public IRenderPass
    {
    public:
        std::string_view GetName() const noexcept override { return "ForwardTransparentPass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}