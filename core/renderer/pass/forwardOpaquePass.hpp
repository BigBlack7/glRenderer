#pragma once
#include "renderpass.hpp"

namespace core
{
    class ForwardOpaquePass final : public IRenderPass
    {
    public:
        std::string_view GetName() const noexcept override { return "ForwardOpaquePass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}