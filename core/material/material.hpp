#pragma once
#include "renderer/renderOption.hpp"
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace core
{
    class Shader;
    class Texture;

    enum class TextureSlot : uint8_t
    {
        Albedo = 0,
        Normal,
        MetallicRoughness,
        AO,
        Emissive,
        OpacityMask,
        Count // 纹理槽数量
    };

    class Material final
    {
    private:
        static constexpr size_t TextureSlotCount = static_cast<size_t>(TextureSlot::Count);
        static_assert(TextureSlotCount <= 32, "TextureSlotCount exceeds 32-bit feature flag capacity.");

        std::shared_ptr<Shader> mShader{};
        std::array<std::shared_ptr<Texture>, TextureSlotCount> mTextures{};
        std::array<std::string, TextureSlotCount> mTextureUniformNames{
            "uAlbedoSampler",        // Albedo
            "uNormalSampler",        // Normal
            "uMetallicRoughSampler", // MetallicRoughness(for Blinn-Phong -> SpecularMask)
            "uAOSampler",            // AO
            "uEmissiveSampler",      // Emissive
            "uOpacitySampler"       // OpacityMask
        };

        // 材质特性位掩码
        uint32_t mFeatureFlags{0};
        // 材质版本号, 用于缓存管理
        uint64_t mVersion{1};
        // 渲染状态描述
        RenderStateDesc mRenderState{MakeOpaqueState()};

        std::unordered_map<std::string, float> mFloatParams{};
        std::unordered_map<std::string, int> mIntParams{};
        std::unordered_map<std::string, uint32_t> mUIntParams{};
        std::unordered_map<std::string, glm::vec3> mVec3Params{};

        // 运行期日志记录避免刷屏
        mutable bool mWarnedNoShader{false};          // 记录是否已经警告过缺失着色器
        mutable uint32_t mWarnedEmptySamplerMask{0u}; // 记录是否已经警告过缺失贴图槽位

    private:
        static constexpr size_t ToIndex(TextureSlot slot) { return static_cast<size_t>(slot); }

        /// @brief 根据当前贴图槽重建位掩码, 数据变更后保持一致性
        void RebuildFeatureFlags();

        void BumpVersion() noexcept { ++mVersion; }

    public:
        Material() = default;
        explicit Material(std::shared_ptr<Shader> shader);

        /// @brief 设置着色器
        /// @param shader 着色器对象的shared_ptr, 允许外部共享和复用着色器资源
        void SetShader(std::shared_ptr<Shader> shader);
        const std::shared_ptr<Shader> &GetShader() const { return mShader; }

        /// @brief 设置纹理, 通过TextureSlot指定槽位, 允许外部共享和复用纹理资源
        /// @param slot 纹理槽位
        /// @param texture 纹理对象
        void SetTexture(TextureSlot slot, std::shared_ptr<Texture> texture);

        /// @brief 获取纹理
        /// @param slot 纹理槽位
        /// @return 对应槽位的纹理对象, 如果未设置则返回nullptr
        std::shared_ptr<Texture> GetTexture(TextureSlot slot) const;

        /// @brief 支持与shader命名不一致时手动映射
        /// @param slot 纹理槽位
        /// @param uniformName 纹理采样器在shader中的uniform变量名
        void SetTextureUniformName(TextureSlot slot, std::string uniformName);

        /// @brief 
        /// @param state 
        void SetRenderState(const RenderStateDesc &state);

        /* setter */
        void SetFloat(std::string_view name, float value);
        void SetInt(std::string_view name, int value);
        void SetUInt(std::string_view name, uint32_t value);
        void SetVec3(std::string_view name, const glm::vec3 &value);

        uint32_t GetFeatureFlags() const { return mFeatureFlags; }
        uint64_t GetVersion() const noexcept { return mVersion; }
        const RenderStateDesc &GetRenderState() const noexcept { return mRenderState; }

        /* getter */
        const std::array<std::shared_ptr<Texture>, TextureSlotCount> &GetTextures() const noexcept { return mTextures; }
        const std::array<std::string, TextureSlotCount> &GetTextureUniformNames() const noexcept { return mTextureUniformNames; }
        const std::unordered_map<std::string, float> &GetFloatParams() const noexcept { return mFloatParams; }
        const std::unordered_map<std::string, int> &GetIntParams() const noexcept { return mIntParams; }
        const std::unordered_map<std::string, uint32_t> &GetUIntParams() const noexcept { return mUIntParams; }
        const std::unordered_map<std::string, glm::vec3> &GetVec3Params() const noexcept { return mVec3Params; }
    };
}