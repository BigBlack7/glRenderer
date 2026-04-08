#pragma once
#include "renderpass.hpp"
#include <memory>
#include <array>

namespace core
{
    class Shader;

    enum class ToneMapMode : uint32_t
    {
        Reinhard = 0,
        Exposure = 1
    };

    struct PostProcessSettings final
    {
        bool mGammaEnabled{true};                        // 是否启用伽马校正: 将线性颜色空间转换为sRGB显示空间, 默认启用以确保正确显示
        float mGamma{2.2f};                              // 伽马值: 控制亮度曲线, 2.2为标准sRGB伽马值, 范围建议1.0-3.0, 值越大画面越亮
        ToneMapMode mToneMapMode{ToneMapMode::Exposure}; // 色调映射模式: 控制HDR到LDR的转换方式, Exposure为简单曝光模式
        float mExposure{1.f};                            // 曝光值: 控制整体亮度, 1.0为标准曝光, 范围建议0.1-10.0, 值越大画面越亮
        float mSaturation{1.f};                          // 饱和度: 控制色彩鲜艳程度, 1.0为原始饱和度, 范围建议0.0-2.0, 0.0为黑白, >1.0增强色彩
        float mContrast{1.f};                            // 对比度: 控制明暗差异, 1.0为原始对比度, 范围建议0.5-2.0, 值越大明暗对比越强
        float mVignette{0.f};                            // 暗角强度: 屏幕边缘变暗效果, 0.0为无暗角, 范围建议0.0-1.0, 值越大暗角越明显
        bool mBloomEnabled{true};                        // 是否启用Bloom效果: 高亮度区域的光晕扩散效果, 增强视觉冲击力
        float mBloomIntensity{0.08f};                    // Bloom强度: 控制光晕的可见程度, 范围建议0.01-0.5, 值越大光晕越明显, 过大会导致过曝
        float mBloomThreshold{1.f};                      // Bloom阈值: 只有超过此亮度值的像素才产生Bloom, 范围建议0.5-3.0, 值越小Bloom范围越大
        float mBloomKnee{0.5f};                          // Bloom膝部: 控制亮度过渡的平滑度, 范围建议0.1-1.0, 值越大过渡越平滑, 避免阈值处的硬边缘
        uint32_t mBloomIterations{6u};                   // Bloom模糊迭代次数: 控制光晕扩散范围, 范围建议1-16, 值越大光晕扩散越远但性能开销越大
    };

    class PostProcessPass final : public IRenderPass
    {
    private:
        std::shared_ptr<Shader> mShader{};
        std::shared_ptr<Shader> mBloomExtractShader{};
        std::shared_ptr<Shader> mBloomBlurShader{};

        std::array<GLuint, 2> mBloomFBO{0u, 0u};
        std::array<GLuint, 2> mBloomTexture{0u, 0u};
        uint32_t mBloomWidth{0u};
        uint32_t mBloomHeight{0u};

        bool mInitTried{false};
        static PostProcessSettings sSettings;

        bool EnsureInit();
        bool EnsureBloomResources(uint32_t width, uint32_t height);
        void ReleaseBloomResources() noexcept;

    public:
        ~PostProcessPass() override;

        static void SetGlobalSettings(const PostProcessSettings &settings) noexcept;
        static const PostProcessSettings &GetGlobalSettings() noexcept;

        std::string_view GetName() const noexcept override { return "PostProcessPass"; }
        void Execute(FrameContext &ctx, RenderBackend &backend) override;
    };
}