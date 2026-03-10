#include "rendergraph.hpp"
#include "renderBackend.hpp"

namespace core
{
    void RenderGraph::Reset()
    {
        mPasses.clear();
    }

    void RenderGraph::AddPass(std::unique_ptr<IRenderPass> pass)
    {
        if (pass)
            mPasses.push_back(std::move(pass));
    }

    bool RenderGraph::Compile() const
    {
        // 简单验证至少有一个通道
        return !mPasses.empty();
    }

    void RenderGraph::Execute(FrameContext &context, RenderBackend &backend)
    {
        for (const auto &pass : mPasses)
            pass->Execute(context, backend);
    }
}