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

        // flags与纹理存在性相关, 不依赖shader, 不需要重建FeatureFlags
        mShader = std::move(shader);
        if (mShader)
        {
            mWarnedNoShader = false; // shader状态变化后重置一次性告警
        }

        BumpVersion();
    }

    void Material::SetTexture(TextureSlot slot, std::shared_ptr<Texture> texture)
    {
        const size_t index = ToIndex(slot);

        if (mTextures[index] == texture)
            return;

        mTextures[index] = std::move(texture);

        // 纹理移除后, 清理该槽位告警位, 避免后续无法再次提示
        if (!mTextures[index])
        {
            const uint32_t bit = (1u << static_cast<uint32_t>(index));
            mWarnedEmptySamplerMask &= ~bit;
        }
        RebuildFeatureFlags();
        BumpVersion();
    }

    std::shared_ptr<Texture> Material::GetTexture(TextureSlot slot) const
    {
        return mTextures[ToIndex(slot)];
    }

    void Material::SetTextureUniformName(TextureSlot slot, std::string uniformName)
    {
        // 保存槽位对应sampler名
        const size_t index = ToIndex(slot);

        if (mTextureUniformNames[index] == uniformName)
            return;

        mTextureUniformNames[index] = std::move(uniformName);

        // uniformName修复后清理告警位
        if (!mTextureUniformNames[index].empty())
        {
            const uint32_t bit = (1u << static_cast<uint32_t>(index));
            mWarnedEmptySamplerMask &= ~bit;
        }
        BumpVersion();
    }

    void Material::SetRenderState(const RenderStateDesc &state)
    {
        mRenderState = state;
        BumpVersion();
    }

    void Material::SetFloat(std::string_view name, float value)
    {
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
        const std::string key(name);
        const auto it = mVec3Params.find(key);
        if (it != mVec3Params.end() && glm::length(it->second - value) <= detail::Epsilon)
            return;

        mVec3Params[key] = value;
        BumpVersion();
    }
}