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

    void Material::Bind() const
    {
        if (!mShader)
        {
            if (!mWarnedNoShader)
            {
                GL_ERROR("[Material] Bind Skipped: Shader Is Nullptr");
                mWarnedNoShader = true;
            }
            return;
        }
        mWarnedNoShader = false;

        mShader->Begin();

        for (size_t i = 0; i < TextureSlotCount; ++i)
        {
            const uint32_t bit = (1u << static_cast<uint32_t>(i));
            const auto &tex = mTextures[i];
            if (!tex) // 对应槽位无纹理时, 不绑定也不设置uniform
            {
                /*
                    之前可能有纹理但无uniform名, 现在纹理被移除了清理警告位,
                    这样如果后续又添加纹理但仍无uniform名, 可以再次警告
                */
                mWarnedEmptySamplerMask &= ~bit; // 无纹理时清理告警位
                continue;
            }

            tex->Bind(static_cast<uint32_t>(i));
            const auto &uniformName = mTextureUniformNames[i];

            if (uniformName.empty())
            {
                if ((mWarnedEmptySamplerMask & bit) == 0u) // 该槽位还没警告过, 警告一次后设置对应位, 避免重复警告
                {
                    GL_WARN("[Material] Texture Exists But Sampler Name Is Empty, Slot = {}", i);
                    mWarnedEmptySamplerMask |= bit; // 标记该槽位已警告
                }
                continue;
            }

            // 该槽位已恢复正常(有纹理且有uniform名)清理警告位, 如果后续又变成有纹理但无uniform名, 可以再次警告
            mWarnedEmptySamplerMask &= ~bit;
            
            mShader->SetInt(uniformName, static_cast<int>(i));
        }

        // 可选统一标志位, shader里用来判断是否采样某纹理
        mShader->SetUInt("uMaterialFlags", mFeatureFlags);

        for (const auto &[name, value] : mFloatParams)
            mShader->SetFloat(name, value);
        for (const auto &[name, value] : mIntParams)
            mShader->SetInt(name, value);
        for (const auto &[name, value] : mUIntParams)
            mShader->SetUInt(name, value);
        for (const auto &[name, value] : mVec3Params)
            mShader->SetVec3(name, value);
    }

    void Material::Unbind() const
    {
        if (mShader)
            mShader->End();
    }
}