#include "environmentMap.hpp"
#include <algorithm>

namespace core
{
    EnvironmentMap::EnvironmentMap(const std::array<std::filesystem::path, 6> &cubeFacePaths, const std::filesystem::path &baseDir) : mSourceType(SourceType::CubeMap)
    {
        mTexture = Texture::CreateCubemap(cubeFacePaths, baseDir);
        if (mTexture)
        {
            mDebugName = mTexture->GetDebugName();
            const auto stem = std::filesystem::path(cubeFacePaths[0]).parent_path().filename().string();
            mCacheKey = stem.empty() ? "env_cubemap" : stem;
        }
    }

    EnvironmentMap::EnvironmentMap(const std::filesystem::path &panoramaPath, const std::filesystem::path &baseDir) : mSourceType(SourceType::CubeMap)
    {
        mTexture = Texture::CreateCubemapFromPanorama(panoramaPath, baseDir);
        if (mTexture)
        {
            mDebugName = mTexture->GetDebugName();
            const auto stem = panoramaPath.stem().string();
            mCacheKey = stem.empty() ? "env_panorama" : stem;
        }
    }
}