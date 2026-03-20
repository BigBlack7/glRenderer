#include "material.hpp"
#include "shader/shader.hpp"
#include "texture/texture.hpp"
#include "utils/logger.hpp"
#include <glm/geometric.hpp>

namespace core
{
    namespace detail
    {
        constexpr float Epsilon = 1e-6f;
        constexpr std::string_view ShininessName = "uShininess";
        constexpr std::string_view BaseColorName = "uBaseColor";
        constexpr std::string_view LegacyBaseColorName = "uDefaultColor";
    }

    Material::Material(std::shared_ptr<Shader> shader) : mShader(std::move(shader))
    {
        RebuildFeatureFlags();
    }

    void Material::RebuildFeatureFlags()
    {
        mFeatureFlags = 0u;
        for (size_t i = 0; i < TextureSlotCount; ++i)
        {
            if (mTextures[i])
            {
                mFeatureFlags |= (1u << static_cast<uint32_t>(i));
            }
        }
    }

    void Material::SetShader(std::shared_ptr<Shader> shader)
    {
        if (mShader == shader)
            return;

        mShader = std::move(shader);
        BumpVersion();
    }

    void Material::SetTexture(TextureSlot slot, std::shared_ptr<Texture> texture)
    {
        const size_t index = ToIndex(slot);
        if (mTextures[index] == texture)
            return;

        mTextures[index] = std::move(texture);
        RebuildFeatureFlags();
        BumpVersion();
    }

    std::shared_ptr<Texture> Material::GetTexture(TextureSlot slot) const
    {
        return mTextures[ToIndex(slot)];
    }

    void Material::SetRenderState(const RenderStateDesc &state)
    {
        if (mRenderState == state)
            return;

        mRenderState = state;
        BumpVersion();
    }

    void Material::SetBaseColor(const glm::vec3 &value)
    {
        if (glm::length(mBaseColor - value) <= detail::Epsilon)
            return;

        mBaseColor = value;
        BumpVersion();
    }

    void Material::SetShininess(float value)
    {
        value = std::max(1.f, value);
        if (glm::abs(mShininess - value) <= detail::Epsilon)
            return;

        mShininess = value;
        BumpVersion();
    }

    void Material::SetFloat(std::string_view name, float value)
    {
        if (name == detail::ShininessName)
        {
            SetShininess(value);
            return;
        }

        const std::string key(name);
        const auto it = mFloatParams.find(key);
        if (it != mFloatParams.end() && glm::abs(it->second - value) <= detail::Epsilon)
            return;

        mFloatParams[key] = value;
        BumpVersion();
    }

    void Material::SetInt(std::string_view name, int value)
    {
        const std::string key(name);
        const auto it = mIntParams.find(key);
        if (it != mIntParams.end() && it->second == value)
            return;

        mIntParams[key] = value;
        BumpVersion();
    }

    void Material::SetUInt(std::string_view name, uint32_t value)
    {
        const std::string key(name);
        const auto it = mUIntParams.find(key);
        if (it != mUIntParams.end() && it->second == value)
            return;

        mUIntParams[key] = value;
        BumpVersion();
    }

    void Material::SetVec3(std::string_view name, const glm::vec3 &value)
    {
        if (name == detail::BaseColorName || name == detail::LegacyBaseColorName)
        {
            SetBaseColor(value);
            return;
        }

        const std::string key(name);
        const auto it = mVec3Params.find(key);
        if (it != mVec3Params.end() && glm::length(it->second - value) <= detail::Epsilon)
            return;

        mVec3Params[key] = value;
        BumpVersion();
    }
}