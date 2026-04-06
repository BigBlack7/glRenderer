#include "renderer.hpp"
#include "pass/shadowPass.hpp"
#include "pass/forwardOpaquePass.hpp"
#include "pass/skyboxPass.hpp"
#include "pass/forwardTransparentPass.hpp"
#include "pass/lightProxyPass.hpp"
#include "pass/postProcessPass.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <memory>

namespace core
{
    namespace detail
    {
        constexpr uint32_t SceneMSAASamples = 4u;
    }

    bool Renderer::EnsureInit()
    {
        if (!mInitialized)
            Init();
        return mInitialized && mBackend.IsInitialized();
    }

    bool Renderer::EnsureSceneTarget()
    {
        GLint viewport[4]{0, 0, 0, 0}; // 存储当前视口信息 0~width(2), 0~height(3)
        glGetIntegerv(GL_VIEWPORT, viewport);

        const uint32_t width = static_cast<uint32_t>(std::max(viewport[2], 1));
        const uint32_t height = static_cast<uint32_t>(std::max(viewport[3], 1));

        if (!mSceneColorTarget.IsValid()) // 场景颜色目标不存在
        {
            if (!mSceneColorTarget.Create(width, height, true, detail::SceneMSAASamples)) // 创建场景颜色目标失败
            {
                GL_CRITICAL("[Renderer] Failed To Create Scene Color Target: {}x{}", width, height);
                return false;
            }
        }
        else if (width != mTargetWidth || height != mTargetHeight) // 场景颜色目标尺寸与当前视口不一致
        {
            mSceneColorTarget.Resize(width, height); // 调整场景颜色目标尺寸
            if (!mSceneColorTarget.IsValid())
            {
                GL_CRITICAL("[Renderer] Failed To Resize Scene Color Target: {}x{}", width, height);
                return false;
            }
        }

        mTargetWidth = width;
        mTargetHeight = height;
        return true;
    }

    void Renderer::Init()
    {
        if (mInitialized)
            return;

        if (!mBackend.Init())
        {
            GL_CRITICAL("[Renderer] Init Failed: Backend Init Failed");
            return;
        }

        mGraph.Reset();
        mGraph.AddPass(std::make_unique<ShadowPass>());
        mGraph.AddPass(std::make_unique<ForwardOpaquePass>());
        mGraph.AddPass(std::make_unique<SkyboxPass>());
        mGraph.AddPass(std::make_unique<ForwardTransparentPass>());
        mGraph.AddPass(std::make_unique<LightProxyPass>());
        mGraph.AddPass(std::make_unique<PostProcessPass>());
        PostProcessPass::SetGlobalSettings(mPostProcessSettings);

        if (!mGraph.Compile())
        {
            GL_CRITICAL("[Renderer] Init Failed: Render Graph Compile Failed");
            mBackend.Shutdown();
            return;
        }

        mInitialized = true;
        GL_INFO("[Renderer] GLRenderer Init Succeeded");
    }

    void Renderer::Shutdown()
    {
        mGraph.Reset();
        mSceneColorTarget = FrameBuffer{};
        mTargetWidth = 0;
        mTargetHeight = 0;
        mBackend.Shutdown();
        mInitialized = false;
    }

    void Renderer::Render(Scene &scene, const Camera &camera, float timeSec)
    {
        if (!EnsureInit())
            return;

        if (!EnsureSceneTarget())
            return;

        FrameContext context{};
        context.__scene__ = &scene;
        context.__camera__ = &camera;
        context.__timeSec__ = timeSec;
        context.__sceneColorTarget__ = &mSceneColorTarget;
        context.__targetWidth__ = mTargetWidth;
        context.__targetHeight__ = mTargetHeight;

        scene.UpdateWorldMatrices();
        context.__renderQueue__.Build(scene, camera);

        mBackend.SetClearColor(mClearColor);
        PostProcessPass::SetGlobalSettings(mPostProcessSettings);

        mGraph.Execute(context, mBackend);
        mInfos = context.__stats__;
    }
}