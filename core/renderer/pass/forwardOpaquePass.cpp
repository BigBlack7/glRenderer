#include "forwardOpaquePass.hpp"
#include "renderer/renderBackend.hpp"

namespace core
{
    void ForwardOpaquePass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        if (!ctx.__scene__ || !ctx.__camera__)
            return;

        backend.BeginRenderTarget(nullptr, true);
        backend.UploadFrameBlock(*ctx.__camera__, ctx.__timeSec__);
        backend.UploadLightBlock(ctx.__scene__->GetDirectionalLights(), ctx.__scene__->GetPointLights(), ctx.__scene__->GetSpotLights());
        backend.DrawOpaqueQueue(ctx.__opaqueQueue__, ctx.__stats__);
        backend.EndRenderTarget();
    }
}