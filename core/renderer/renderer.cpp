#include "renderer.hpp"
#include "pass/forwardOpaquePass.hpp"
#include "pass/forwardTransparentPass.hpp"
#include "utils/logger.hpp"
#include <memory>

namespace core
{
    bool Renderer::EnsureInit()
    {
        if (!mInitialized)
            Init();
        return mInitialized && mBackend.IsInitialized();
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
        mGraph.AddPass(std::make_unique<ForwardOpaquePass>());
        mGraph.AddPass(std::make_unique<ForwardTransparentPass>());

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
        mBackend.Shutdown();
        mInitialized = false;
    }

    void Renderer::Render(Scene &scene, const Camera &camera, float timeSec)
    {
        if (!EnsureInit())
            return;

        FrameContext context{};
        context.__scene__ = &scene;
        context.__camera__ = &camera;
        context.__timeSec__ = timeSec;

        scene.UpdateWorldMatrices();
        context.__renderQueue__.Build(scene, camera);

        mBackend.SetClearColor(mClearColor);

        mGraph.Execute(context, mBackend);
        mInfos = context.__stats__;
    }
}