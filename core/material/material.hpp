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
        MetallicRoughness, // for Blinn-Phong -> SpecularMask
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
        

        // 材质特性位掩码
        uint32_t mFeatureFlags{0};
        // 材质版本号, 用于缓存管理
        uint64_t mVersion{1};
        // 渲染状态描述
        RenderStateDesc mRenderState{MakeOpaqueState()};

        glm::vec3 mBaseColor{1.f, 1.f, 1.f};
        float mShininess{64.f};

        // 仅用于额外扩展参数, 核心参数改为强类型字段
        std::unordered_map<std::string, float> mFloatParams{};
        std::unordered_map<std::string, int> mIntParams{};
        std::unordered_map<std::string, uint32_t> mUIntParams{};
        std::unordered_map<std::string, glm::vec3> mVec3Params{};

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

        /// @brief 设置渲染状态
        /// @param state 渲染状态描述
        void SetRenderState(const RenderStateDesc &state);

        void SetBaseColor(const glm::vec3 &value);

        void SetShininess(float value);

        /* setter 识别uBaseColor/uDefaultColor/uShininess, 其余参数进入自定义参数池 */
        void SetFloat(std::string_view name, float value);
        void SetInt(std::string_view name, int value);
        void SetUInt(std::string_view name, uint32_t value);
        void SetVec3(std::string_view name, const glm::vec3 &value);

        uint32_t GetFeatureFlags() const { return mFeatureFlags; }
        uint64_t GetVersion() const noexcept { return mVersion; }
        const RenderStateDesc &GetRenderState() const noexcept { return mRenderState; }

        const glm::vec3 &GetBaseColor() const noexcept { return mBaseColor; }
        float GetShininess() const noexcept { return mShininess; }

        /* getter */
        const std::array<std::shared_ptr<Texture>, TextureSlotCount> &GetTextures() const noexcept { return mTextures; }
        const std::unordered_map<std::string, float> &GetFloatParams() const noexcept { return mFloatParams; }
        const std::unordered_map<std::string, int> &GetIntParams() const noexcept { return mIntParams; }
        const std::unordered_map<std::string, uint32_t> &GetUIntParams() const noexcept { return mUIntParams; }
        const std::unordered_map<std::string, glm::vec3> &GetVec3Params() const noexcept { return mVec3Params; }
    };
}