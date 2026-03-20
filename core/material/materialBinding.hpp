#pragma once
#include "material.hpp"
#include <glad/glad.h>
#include <array>

namespace core
{
    class Shader;

    struct MaterialShaderBindings final
    {
        std::array<GLint, static_cast<size_t>(TextureSlot::Count)> mSampler{};
        GLint mMaterialFlags{-1};
        GLint mAlphaMode{-1};
        GLint mAlphaCutoff{-1};
        GLint mOpacity{-1};
        GLint mBaseColor{-1};
        GLint mShininess{-1};

        MaterialShaderBindings()
        {
            mSampler.fill(-1);
        }
    };

    MaterialShaderBindings BuildMaterialShaderBindings(const Shader &shader);
}