#include "postProcessPass.hpp"
#include "renderer/renderBackend.hpp"
#include "renderer/buffer/frameBuffer.hpp"
#include "shader/shader.hpp"

namespace core
{
    PostProcessSettings PostProcessPass::sSettings{};

    void PostProcessPass::SetGlobalSettings(const PostProcessSettings &settings) noexcept
    {
        sSettings = settings;
    }

    const PostProcessSettings &PostProcessPass::GetGlobalSettings() noexcept
    {
        return sSettings;
    }

    bool PostProcessPass::EnsureInit()
    {
        if (mShader && mShader->GetID() != 0)
            return true;

        if (mInitTried) // 初始化尝试过, 直接返回失败, 避免每帧重复失败初始化
            return false;

        mInitTried = true;
        mShader = std::make_shared<Shader>("pass/postprocess.vert", "pass/postprocess.frag");
        return mShader && mShader->GetID() != 0;
    }

    void PostProcessPass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        if (!ctx.__sceneColorTarget__ || !ctx.__sceneColorTarget__->IsValid())
            return;

        if (!EnsureInit())
            return;

        // 后处理采样器使用sampler2D, MSAA目标需先解析到单采样颜色纹理
        ctx.__sceneColorTarget__->ResolveColor();

        backend.BeginRenderTarget(nullptr, false, false, false);

        if (ctx.__targetWidth__ > 0u && ctx.__targetHeight__ > 0u)
            glViewport(0, 0, static_cast<GLsizei>(ctx.__targetWidth__), static_cast<GLsizei>(ctx.__targetHeight__));

        RenderStateDesc postState = MakeOpaqueState();
        postState.mDepth.mDepthTest = false;
        postState.mDepth.mDepthWrite = false;
        postState.mStencil.mStencilTest = false;
        postState.mBlend.mBlend = false;
        postState.mRaster.mFaceCull = false;
        backend.ApplyPassState(postState);

        glUseProgram(mShader->GetID());
        mShader->SetUIntOptional("uGammaEnabled", sSettings.mGammaEnabled ? 1u : 0u);
        mShader->SetFloatOptional("uGamma", sSettings.mGamma);
        mShader->SetFloatOptional("uExposure", sSettings.mExposure);
        mShader->SetFloatOptional("uSaturation", sSettings.mSaturation);
        mShader->SetFloatOptional("uContrast", sSettings.mContrast);
        mShader->SetFloatOptional("uVignette", sSettings.mVignette);

        backend.DrawFullscreenTexture(*mShader, ctx.__sceneColorTarget__->GetColorAttachment(), ctx.__stats__);
        backend.EndRenderTarget();
    }
}