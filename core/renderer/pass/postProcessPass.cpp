#include "postProcessPass.hpp"
#include "renderer/renderBackend.hpp"
#include "renderer/buffer/frameBuffer.hpp"
#include "shader/shader.hpp"
#include <algorithm>

namespace core
{
    PostProcessSettings PostProcessPass::sSettings{};

    PostProcessPass::~PostProcessPass()
    {
        ReleaseBloomResources();
    }

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
        if (mShader && mShader->GetID() != 0 &&
            mBloomExtractShader && mBloomExtractShader->GetID() != 0 &&
            mBloomBlurShader && mBloomBlurShader->GetID() != 0)
            return true;

        if (mInitTried) // 初始化尝试过, 直接返回失败, 避免每帧重复失败初始化
            return false;

        mInitTried = true;
        mShader = std::make_shared<Shader>("pass/postprocess.vert", "pass/postprocess.frag");
        mBloomExtractShader = std::make_shared<Shader>("pass/postprocess.vert", "pass/bloomExtract.frag");
        mBloomBlurShader = std::make_shared<Shader>("pass/postprocess.vert", "pass/bloomBlur.frag");

        return mShader && mShader->GetID() != 0 &&
               mBloomExtractShader && mBloomExtractShader->GetID() != 0 &&
               mBloomBlurShader && mBloomBlurShader->GetID() != 0;
    }

    void PostProcessPass::ReleaseBloomResources() noexcept
    {
        if (mBloomTexture[0] != 0 || mBloomTexture[1] != 0)
            glDeleteTextures(2, mBloomTexture.data());

        if (mBloomFBO[0] != 0 || mBloomFBO[1] != 0)
            glDeleteFramebuffers(2, mBloomFBO.data());

        mBloomTexture = {0u, 0u};
        mBloomFBO = {0u, 0u};
        mBloomWidth = 0u;
        mBloomHeight = 0u;
    }

    bool PostProcessPass::EnsureBloomResources(uint32_t width, uint32_t height)
    {
        const uint32_t targetW = std::max(1u, width / 2u);
        const uint32_t targetH = std::max(1u, height / 2u);

        if (mBloomFBO[0] != 0 && mBloomFBO[1] != 0 &&
            mBloomTexture[0] != 0 && mBloomTexture[1] != 0 &&
            mBloomWidth == targetW && mBloomHeight == targetH)
            return true;

        ReleaseBloomResources();

        glGenFramebuffers(2, mBloomFBO.data());
        glGenTextures(2, mBloomTexture.data());

        for (int i = 0; i < 2; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, mBloomTexture[i]);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA16F,
                static_cast<GLsizei>(targetW),
                static_cast<GLsizei>(targetH),
                0,
                GL_RGBA,
                GL_FLOAT,
                nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 设置缩小过滤为线性插值, 保证模糊效果平滑
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // 设置S方向纹理坐标为边缘夹取, 避免模糊时边缘采样到对侧
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glBindFramebuffer(GL_FRAMEBUFFER, mBloomFBO[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBloomTexture[i], 0);

            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                ReleaseBloomResources();
                return false;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        mBloomWidth = targetW;
        mBloomHeight = targetH;
        return true;
    }

    void PostProcessPass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        if (!ctx.__sceneColorTarget__ || !ctx.__sceneColorTarget__->IsValid())
            return;

        if (!EnsureInit())
            return;

        if (!EnsureBloomResources(ctx.__targetWidth__, ctx.__targetHeight__))
            return;

        // 后处理采样器使用sampler2D, MSAA目标需先解析到单采样颜色纹理
        ctx.__sceneColorTarget__->ResolveColor();

        RenderStateDesc postState = MakeOpaqueState();
        postState.mDepth.mDepthTest = false;
        postState.mDepth.mDepthWrite = false;
        postState.mStencil.mStencilTest = false;
        postState.mBlend.mBlend = false;
        postState.mRaster.mFaceCull = false;
        backend.ApplyPassState(postState);

        GLuint bloomCompositeTex = 0u; // 初始化Bloom合成纹理为0, 如果Bloom禁用则保持为0, 后续合成时会跳过Bloom混合
        if (sSettings.mBloomEnabled)
        {
            // 1) bright pass
            glBindFramebuffer(GL_FRAMEBUFFER, mBloomFBO[0]); // 绑定第一个Bloom FBO作为渲染目标, 准备绘制亮度提取结果
            glViewport(0, 0, static_cast<GLsizei>(mBloomWidth), static_cast<GLsizei>(mBloomHeight));
            glClear(GL_COLOR_BUFFER_BIT); // 清空颜色缓冲, 清除上一帧的Bloom数据, 避免残影

            glUseProgram(mBloomExtractShader->GetID());
            mBloomExtractShader->SetFloatOptional("uThreshold", sSettings.mBloomThreshold);
            mBloomExtractShader->SetFloatOptional("uKnee", sSettings.mBloomKnee);
            backend.DrawFullscreenTexture(*mBloomExtractShader, ctx.__sceneColorTarget__->GetColorAttachment(), ctx.__stats__);

            // 2) blur ping-pong
            GLuint inputTex = mBloomTexture[0];
            uint32_t blurPasses = std::clamp<uint32_t>(sSettings.mBloomIterations, 1u, 16u);
            bool horizontal = true;
            for (uint32_t i = 0; i < blurPasses; ++i)
            {
                const int targetIndex = horizontal ? 1 : 0; // 根据当前方向选择目标FBO索引
                glBindFramebuffer(GL_FRAMEBUFFER, mBloomFBO[targetIndex]);
                glViewport(0, 0, static_cast<GLsizei>(mBloomWidth), static_cast<GLsizei>(mBloomHeight));
                glClear(GL_COLOR_BUFFER_BIT);

                glUseProgram(mBloomBlurShader->GetID());
                mBloomBlurShader->SetUIntOptional("uHorizontal", horizontal ? 1u : 0u);
                backend.DrawFullscreenTexture(*mBloomBlurShader, inputTex, ctx.__stats__);

                inputTex = mBloomTexture[targetIndex];
                horizontal = !horizontal;
            }
            bloomCompositeTex = inputTex;
        }

        backend.BeginRenderTarget(nullptr, false, false, false);

        if (ctx.__targetWidth__ > 0u && ctx.__targetHeight__ > 0u)
            glViewport(0, 0, static_cast<GLsizei>(ctx.__targetWidth__), static_cast<GLsizei>(ctx.__targetHeight__));

        backend.ApplyPassState(postState);

        glUseProgram(mShader->GetID());
        mShader->SetUIntOptional("uGammaEnabled", sSettings.mGammaEnabled ? 1u : 0u);
        mShader->SetUIntOptional("uToneMapMode", static_cast<uint32_t>(sSettings.mToneMapMode));
        mShader->SetFloatOptional("uGamma", sSettings.mGamma);
        mShader->SetFloatOptional("uExposure", sSettings.mExposure);
        mShader->SetFloatOptional("uSaturation", sSettings.mSaturation);
        mShader->SetFloatOptional("uContrast", sSettings.mContrast);
        mShader->SetFloatOptional("uVignette", sSettings.mVignette);
        mShader->SetUIntOptional("uBloomEnabled", (sSettings.mBloomEnabled && bloomCompositeTex != 0u) ? 1u : 0u);
        mShader->SetFloatOptional("uBloomIntensity", sSettings.mBloomIntensity);
        glBindTextureUnit(1u, bloomCompositeTex);
        mShader->SetIntOptional("uBloomSampler", 1);

        backend.DrawFullscreenTexture(*mShader, ctx.__sceneColorTarget__->GetColorAttachment(), ctx.__stats__);
        backend.EndRenderTarget();
    }
}