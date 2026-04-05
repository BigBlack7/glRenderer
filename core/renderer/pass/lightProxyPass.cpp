#include "lightProxyPass.hpp"
#include "renderer/renderBackend.hpp"
#include "geometry/mesh.hpp"
#include "light/light.hpp"
#include "shader/shader.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace core
{
    bool LightProxyPass::EnsureInit()
    {
        if (mProxyShader && mProxyShader->GetID() != 0 && mSphere && mSphere->IsValid())
            return true;

        if (mInitTried)
            return false;

        mInitTried = true;
        mProxyShader = std::make_shared<Shader>("pass/lightProxy.vert", "pass/lightProxy.frag");
        mSphere = Mesh::CreateSphere(1.f, 16u, 16u);
        return mProxyShader && mProxyShader->GetID() != 0 && mSphere && mSphere->IsValid();
    }

    void LightProxyPass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        if (!ctx.__scene__ || !ctx.__camera__)
            return;

        if (!EnsureInit())
            return;

        backend.BeginRenderTarget(ctx.__sceneColorTarget__, false, false, false);

        RenderStateDesc state = MakeOpaqueState();
        state.mDepth.mDepthTest = true;
        state.mDepth.mDepthWrite = false;
        state.mBlend.mBlend = false;
        state.mStencil.mStencilTest = false;
        state.mRaster.mFaceCull = true;
        state.mRaster.mCullFace = CullMode::Back;
        backend.ApplyPassState(state);

        const GLuint program = mProxyShader->GetID();
        glUseProgram(program);
        ++ctx.__stats__.__programBinds__;

        const glm::mat4 vp = ctx.__camera__->GetProjectionMatrix() * ctx.__camera__->GetViewMatrix();
        mProxyShader->SetMat4("uVP", vp);

        glBindVertexArray(mSphere->GetVAO());

        auto DrawProxySphere = [&](const glm::vec3 &position, const glm::vec3 &color)
        {
            const glm::mat4 model = glm::translate(glm::mat4(1.f), position) * glm::scale(glm::mat4(1.f), glm::vec3(0.12f));
            mProxyShader->SetMat4("uM", model);
            mProxyShader->SetVec3("uColor", glm::clamp(color, glm::vec3(0.f), glm::vec3(16.f)));

            if (mSphere->GetIndexCount() > 0u)
            {
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mSphere->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
                ctx.__stats__.__triangles__ += mSphere->GetIndexCount() / 3u;
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mSphere->GetVertexCount()));
                ctx.__stats__.__triangles__ += mSphere->GetVertexCount() / 3u;
            }

            ++ctx.__stats__.__drawCalls__;
        };

        for (const auto &light : ctx.__scene__->GetPointLights())
        {
            if (!light.IsEnabled() || light.GetIntensity() <= 0.f)
                continue;

            DrawProxySphere(light.GetPosition(), light.GetColor() * light.GetIntensity());
        }

        for (const auto &light : ctx.__scene__->GetSpotLights())
        {
            if (!light.IsEnabled() || light.GetIntensity() <= 0.f)
                continue;

            DrawProxySphere(light.GetPosition(), light.GetColor() * light.GetIntensity());
        }

        glBindVertexArray(0);
        backend.EndRenderTarget();
    }
}
