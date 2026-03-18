#include "environmentMap.hpp"

namespace core
{
    EnvironmentMap::EnvironmentMap(const std::array<std::filesystem::path, 6> &cubeFacePaths, const std::filesystem::path &baseDir) : mSourceType(SourceType::CubeMap)
    {
        mTexture = Texture::CreateCubemap(cubeFacePaths, baseDir);
        if (mTexture)
            mDebugName = mTexture->GetDebugName();
    }

    EnvironmentMap::EnvironmentMap(const std::filesystem::path &panoramaPath, const std::filesystem::path &baseDir) : mSourceType(SourceType::Panorama)
    {
        mTexture = Texture::CreatePanorama(panoramaPath, baseDir);
        if (mTexture)
            mDebugName = mTexture->GetDebugName();
    }
}