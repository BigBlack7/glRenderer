#pragma once
#include "renderpass.hpp"
#include <memory>

namespace core
{
    class Shader;

    struct PostProcessSettings final
    {
        bool mGammaEnabled{true};
        float mGamma{2.2f};
        float mExposure{1.f};
        float mSaturation{1.f};
        float mContrast{1.f};
        float mVignette{0.f};
    };

    class PostProcessPass final : public IRenderPass
    {
    private:
        std::shared_ptr<Shader> mShader{};
        bool mInitTried{false};
        static PostProcessSettings sSettings;

        bool EnsureInit();

    public:
        static void SetGlobalSettings(const PostProcessSettings &settings) noexcept;
        static const PostProcessSettings &GetGlobalSettings() noexcept;

        std::string_view GetName() const noexcept override { return "PostProcessPass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}