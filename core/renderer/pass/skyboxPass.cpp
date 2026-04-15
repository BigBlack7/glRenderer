#include "skyboxPass.hpp"
#include "geometry/mesh.hpp"
#include "renderer/renderBackend.hpp"
#include "texture/environmentMap.hpp"
#include "scene/scene.hpp"
#include "shader/shader.hpp"

namespace core
{
    bool SkyboxPass::EnsureInit()
    {
        if (mShader && mShader->GetID() != 0 && mSphere && mSphere->IsValid())
            return true;

        if (mInitTried)
            return false;

        mInitTried = true;
        mShader = std::make_shared<Shader>("pass/skybox.vert", "pass/skybox.frag");
        mSphere = Mesh::CreateSphere(1.f, 64u, 64u);

        return mShader && mShader->GetID() != 0 && mSphere && mSphere->IsValid();
    }

    void SkyboxPass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        if (!ctx.__scene__ || !ctx.__camera__)
            return;

        if (!ctx.__scene__->IsSkyboxEnabled())
            return;

        const auto &skybox = ctx.__scene__->GetSkybox();
        if (!skybox || !skybox->IsValid())
            return;

        if (!EnsureInit())
            return;

        backend.BeginRenderTarget(ctx.__sceneColorTarget__, false, false, false);

        RenderStateDesc skyState = MakeOpaqueState();
        skyState.mDepth.mDepthTest = true;
        skyState.mDepth.mDepthFunc = CompareOp::LessEqual;
        skyState.mDepth.mDepthWrite = false;
        skyState.mStencil.mStencilTest = false;
        skyState.mBlend.mBlend = false;
        skyState.mRaster.mFaceCull = false;
        backend.ApplyPassState(skyState);

        backend.UploadFrameBlock(*ctx.__camera__, ctx.__timeSec__);
        backend.DrawSkybox(*mShader, *mSphere, *skybox, ctx.__scene__->GetSkyboxIntensity(), ctx.__stats__);

        backend.EndRenderTarget();
    }
}