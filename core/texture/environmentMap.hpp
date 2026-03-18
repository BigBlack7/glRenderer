#pragma once
#include "texture.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace core
{
    class EnvironmentMap final
    {
    public:
        enum class SourceType : uint8_t
        {
            CubeMap = 0,
            Panorama = 1
        };

    private:
        std::shared_ptr<Texture> mTexture{};
        SourceType mSourceType{SourceType::CubeMap};
        std::string mDebugName{};

    public:
        EnvironmentMap() = default;
        ~EnvironmentMap() = default;

        // +X(right), -X(left), +Y(top), -Y(bottom), +Z(front), -Z(back)
        EnvironmentMap(const std::array<std::filesystem::path, 6> &cubeFacePaths, const std::filesystem::path &baseDir = {});

        // 全景图
        EnvironmentMap(const std::filesystem::path &panoramaPath, const std::filesystem::path &baseDir = {});

        EnvironmentMap(const EnvironmentMap &) = delete;
        EnvironmentMap &operator=(const EnvironmentMap &) = delete;

        EnvironmentMap(EnvironmentMap &&other) noexcept = default;
        EnvironmentMap &operator=(EnvironmentMap &&other) noexcept = default;

        bool IsValid() const noexcept { return mTexture && mTexture->IsValid(); }
        bool IsCubeMap() const noexcept { return mSourceType == SourceType::CubeMap; }
        SourceType GetSourceType() const noexcept { return mSourceType; }

        GLuint GetID() const noexcept { return mTexture ? mTexture->GetID() : 0u; }
        uint32_t GetWidth() const noexcept { return mTexture ? mTexture->GetWidth() : 0u; }
        uint32_t GetHeight() const noexcept { return mTexture ? mTexture->GetHeight() : 0u; }
        const std::string &GetDebugName() const noexcept { return mDebugName; }

        const std::shared_ptr<Texture> &GetTexture() const noexcept { return mTexture; }
    };
}