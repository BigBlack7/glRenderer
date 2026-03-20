#include "materialBinding.hpp"
#include "shader/shader.hpp"
#include <initializer_list>
#include <string_view>

namespace core
{
    namespace detail
    {
        GLint FindFirstUniform(const Shader &shader, std::initializer_list<std::string_view> names)
        {
            for (const auto name : names)
            {
                const GLint location = shader.FindUniformLocation(name);
                if (location >= 0)
                    return location;
            }
            return -1;
        }
    }

    MaterialShaderBindings BuildMaterialShaderBindings(const Shader &shader)
    {
        MaterialShaderBindings b{};

        b.mSampler[static_cast<size_t>(TextureSlot::Albedo)] =
            detail::FindFirstUniform(shader, {"uAlbedoSampler", "uBaseColorSampler"});
        b.mSampler[static_cast<size_t>(TextureSlot::Normal)] =
            detail::FindFirstUniform(shader, {"uNormalSampler", "uNormalMapSampler"});
        b.mSampler[static_cast<size_t>(TextureSlot::MetallicRoughness)] =
            detail::FindFirstUniform(shader, {"uMetallicRoughSampler", "uSpecularSampler"});
        b.mSampler[static_cast<size_t>(TextureSlot::AO)] =
            detail::FindFirstUniform(shader, {"uAOSampler", "uAoSampler"});
        b.mSampler[static_cast<size_t>(TextureSlot::Emissive)] =
            detail::FindFirstUniform(shader, {"uEmissiveSampler"});
        b.mSampler[static_cast<size_t>(TextureSlot::OpacityMask)] =
            detail::FindFirstUniform(shader, {"uOpacitySampler", "uAlphaMaskSampler"});

        b.mMaterialFlags = detail::FindFirstUniform(shader, {"uMaterialFlags"});
        b.mAlphaMode = detail::FindFirstUniform(shader, {"uAlphaMode"});
        b.mAlphaCutoff = detail::FindFirstUniform(shader, {"uAlphaCutoff"});
        b.mOpacity = detail::FindFirstUniform(shader, {"uOpacity"});
        b.mBaseColor = detail::FindFirstUniform(shader, {"uBaseColor", "uDefaultColor"});
        b.mShininess = detail::FindFirstUniform(shader, {"uShininess"});

        return b;
    }
}