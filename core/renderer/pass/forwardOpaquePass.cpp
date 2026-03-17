#include "forwardOpaquePass.hpp"
#include "renderer/renderBackend.hpp"

namespace core
{
    void ForwardOpaquePass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        if (!ctx.__scene__ || !ctx.__camera__)
            return;

        backend.BeginRenderTarget(nullptr, true, true, true);

        RenderStateDesc opaquePassState = MakeOpaqueState();
        opaquePassState.mStencil.mStencilTest = false; // 基线关闭模板, 具体物体按材质打开
        backend.ApplyPassState(opaquePassState);

        backend.UploadFrameBlock(*ctx.__camera__, ctx.__timeSec__);
        backend.UploadLightBlock(ctx.__scene__->GetDirectionalLights(), ctx.__scene__->GetPointLights(), ctx.__scene__->GetSpotLights());
        backend.DrawOpaqueQueue(ctx.__renderQueue__, ctx.__stats__);
        backend.EndRenderTarget();
    }
}