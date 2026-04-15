#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace core
{
    class EnvironmentMap;
    class Mesh;
    class Shader;

    class IblPrecompute final
    {
    private:
        struct RuntimeCacheEntry final
        {
            GLuint mIrradiance{0};
            GLuint mPrefilter{0};
            GLuint mBrdfLut{0};
        };

        std::shared_ptr<Mesh> mCaptureMesh{};
        std::shared_ptr<Shader> mIrradianceShader{};
        std::shared_ptr<Shader> mPrefilterShader{};
        std::shared_ptr<Shader> mBrdfLutShader{};
        GLuint mCaptureFbo{0};
        GLuint mCaptureRbo{0};
        GLuint mFullscreenVao{0};
        bool mInitTried{false};

        std::unordered_map<std::string, RuntimeCacheEntry> mRuntimeCache{};

    private:
        bool EnsureInit();
        void Shutdown();

        static std::string MakeCacheKey(const EnvironmentMap &env);
        static std::filesystem::path CacheRoot();
        static std::filesystem::path CacheDirForKey(const std::string &cacheKey);

        static std::array<const char *, 6> FaceShortNames();
        static std::array<glm::mat4, 6> CaptureViews();
        static glm::mat4 CaptureProjection();

        bool TryLoadFromDisk(const std::string &cacheKey, RuntimeCacheEntry &outEntry);
        bool SaveToDisk(const std::string &cacheKey, const RuntimeCacheEntry &entry);

        static GLuint CreateCubemapRGBA16F(uint32_t size, uint32_t mipLevels, bool generateMipmap);
        static GLuint CreateBrdfLutRG16F(uint32_t width, uint32_t height);

        static bool ReadCubemapFace(GLuint textureId, uint32_t mipLevel, uint32_t face, int width, int height, std::vector<float> &outRgba);
        static bool ReadBrdfLut(GLuint textureId, int width, int height, std::vector<float> &outRg);

        bool ComputeIrradiance(GLuint sourceEnvCube, GLuint irradianceCube, uint32_t size);
        bool ComputePrefilter(GLuint sourceEnvCube, GLuint prefilterCube, uint32_t baseSize, uint32_t mipLevels);
        bool ComputeBrdfLut(GLuint brdfLutTex, uint32_t width, uint32_t height);

    public:
        IblPrecompute() = default;
        ~IblPrecompute();

        bool Prepare(EnvironmentMap &env);
    };
}
