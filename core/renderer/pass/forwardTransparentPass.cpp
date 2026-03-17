#include "forwardTransparentPass.hpp"
#include "renderer/renderBackend.hpp"

namespace core
{
    void ForwardTransparentPass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        if (!ctx.__scene__ || !ctx.__camera__)
            return;

        backend.BeginRenderTarget(nullptr, false, false, false);

        RenderStateDesc transparentPassState = MakeTransparentState();
        transparentPassState.mStencil.mStencilTest = false; // 基线关闭模板, 具体物体按材质打开
        backend.ApplyPassState(transparentPassState);

        backend.UploadFrameBlock(*ctx.__camera__, ctx.__timeSec__);
        backend.UploadLightBlock(ctx.__scene__->GetDirectionalLights(), ctx.__scene__->GetPointLights(), ctx.__scene__->GetSpotLights());
        backend.DrawTransparentQueue(ctx.__renderQueue__, ctx.__stats__);
        backend.EndRenderTarget();
    }
}