#include "iblPrecompute.hpp"

#include "geometry/mesh.hpp"
#include "shader/shader.hpp"
#include "texture/environmentMap.hpp"
#include "texture/exrIO.hpp"
#include "utils/logger.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>

namespace core
{
    namespace
    {
        constexpr uint32_t IrradianceSize = 32u;
        constexpr uint32_t PrefilterBaseSize = 128u;
        constexpr uint32_t PrefilterMipLevels = 5u;
        constexpr uint32_t BrdfLutSize = 256u;
    }

    IblPrecompute::~IblPrecompute()
    {
        Shutdown();
    }

    bool IblPrecompute::EnsureInit()
    {
        if (mCaptureMesh && mIrradianceShader && mPrefilterShader && mBrdfLutShader && mCaptureFbo != 0u && mCaptureRbo != 0u)
            return true;

        if (mInitTried)
            return false;

        mInitTried = true;

        mCaptureMesh = Mesh::CreateCube(1.0f);
        mIrradianceShader = std::make_shared<Shader>("pass/ibl/capture.vert", "pass/ibl/irradiance.frag");
        mPrefilterShader = std::make_shared<Shader>("pass/ibl/capture.vert", "pass/ibl/prefilter.frag");
        mBrdfLutShader = std::make_shared<Shader>("pass/ibl/brdfLut.vert", "pass/ibl/brdfLut.frag");

        glGenFramebuffers(1, &mCaptureFbo);
        glGenRenderbuffers(1, &mCaptureRbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mCaptureFbo);
        glBindRenderbuffer(GL_RENDERBUFFER, mCaptureRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(PrefilterBaseSize), static_cast<GLsizei>(PrefilterBaseSize));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mCaptureRbo);

        glGenVertexArrays(1, &mFullscreenVao);

        const bool ok = (mCaptureMesh && mCaptureMesh->IsValid()) &&
                        (mIrradianceShader && mIrradianceShader->GetID() != 0u) &&
                        (mPrefilterShader && mPrefilterShader->GetID() != 0u) &&
                        (mBrdfLutShader && mBrdfLutShader->GetID() != 0u) &&
                        (mCaptureFbo != 0u) && (mCaptureRbo != 0u) && (mFullscreenVao != 0u);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        return ok;
    }

    void IblPrecompute::Shutdown()
    {
        for (auto &it : mRuntimeCache)
        {
            if (it.second.mIrradiance != 0u)
                glDeleteTextures(1, &it.second.mIrradiance);
            if (it.second.mPrefilter != 0u)
                glDeleteTextures(1, &it.second.mPrefilter);
            if (it.second.mBrdfLut != 0u)
                glDeleteTextures(1, &it.second.mBrdfLut);
        }
        mRuntimeCache.clear();

        if (mCaptureFbo != 0u)
            glDeleteFramebuffers(1, &mCaptureFbo);
        if (mCaptureRbo != 0u)
            glDeleteRenderbuffers(1, &mCaptureRbo);
        if (mFullscreenVao != 0u)
            glDeleteVertexArrays(1, &mFullscreenVao);

        mCaptureFbo = 0u;
        mCaptureRbo = 0u;
        mFullscreenVao = 0u;

        mCaptureMesh.reset();
        mIrradianceShader.reset();
        mPrefilterShader.reset();
        mBrdfLutShader.reset();
    }

    std::array<const char *, 6> IblPrecompute::FaceShortNames()
    {
        return {"px", "nx", "py", "ny", "pz", "nz"};
    }

    std::array<glm::mat4, 6> IblPrecompute::CaptureViews()
    {
        return {
            glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};
    }

    glm::mat4 IblPrecompute::CaptureProjection()
    {
        return glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    }

    std::filesystem::path IblPrecompute::CacheRoot()
    {
        return std::filesystem::path("../../../assets/env/cache");
    }

    std::filesystem::path IblPrecompute::CacheDirForKey(const std::string &cacheKey)
    {
        return CacheRoot() / cacheKey;
    }

    std::string IblPrecompute::MakeCacheKey(const EnvironmentMap &env)
    {
        std::string key = env.GetCacheKey();
        if (key.empty())
            key = std::filesystem::path(env.GetDebugName()).stem().string();
        if (key.empty())
            key = "env_default";

        for (char &c : key)
        {
            const bool ok = (c >= 'a' && c <= 'z') ||
                            (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9') ||
                            (c == '_') || (c == '-') || (c == '.');
            if (!ok)
                c = '_';
        }
        return key;
    }

    GLuint IblPrecompute::CreateCubemapRGBA16F(uint32_t size, uint32_t mipLevels, bool generateMipmap)
    {
        GLuint tex = 0u;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            const uint32_t mipSize = std::max(1u, size >> mip);
            for (uint32_t face = 0; face < 6u; ++face)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                             static_cast<GLint>(mip),
                             GL_RGBA16F,
                             static_cast<GLsizei>(mipSize),
                             static_cast<GLsizei>(mipSize),
                             0,
                             GL_RGBA,
                             GL_FLOAT,
                             nullptr);
            }
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, generateMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

        if (generateMipmap)
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return tex;
    }

    GLuint IblPrecompute::CreateBrdfLutRG16F(uint32_t width, uint32_t height)
    {
        GLuint tex = 0u;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    bool IblPrecompute::ReadCubemapFace(GLuint textureId, uint32_t mipLevel, uint32_t face, int width, int height, std::vector<float> &outRgba)
    {
        outRgba.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0.0f);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
        glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                      static_cast<GLint>(mipLevel),
                      GL_RGBA,
                      GL_FLOAT,
                      outRgba.data());
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return true;
    }

    bool IblPrecompute::ReadBrdfLut(GLuint textureId, int width, int height, std::vector<float> &outRg)
    {
        outRg.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 2u, 0.0f);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, outRg.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    bool IblPrecompute::ComputeIrradiance(GLuint sourceEnvCube, GLuint irradianceCube, uint32_t size)
    {
        const auto views = CaptureViews();
        const glm::mat4 proj = CaptureProjection();

        glBindFramebuffer(GL_FRAMEBUFFER, mCaptureFbo);
        glBindRenderbuffer(GL_RENDERBUFFER, mCaptureRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(size), static_cast<GLsizei>(size));

        glViewport(0, 0, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
        glUseProgram(mIrradianceShader->GetID());
        mIrradianceShader->SetMat4("uProjection", proj);
        mIrradianceShader->SetInt("uEnvironmentMap", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, sourceEnvCube);

        glBindVertexArray(mCaptureMesh->GetVAO());

        for (uint32_t face = 0; face < 6u; ++face)
        {
            mIrradianceShader->SetMat4("uView", views[face]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, irradianceCube, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (mCaptureMesh->GetIndexCount() > 0u)
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mCaptureMesh->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
            else
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mCaptureMesh->GetVertexCount()));
        }

        glBindVertexArray(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    bool IblPrecompute::ComputePrefilter(GLuint sourceEnvCube, GLuint prefilterCube, uint32_t baseSize, uint32_t mipLevels)
    {
        const auto views = CaptureViews();
        const glm::mat4 proj = CaptureProjection();

        GLint envBaseSize = 0;
        glBindTexture(GL_TEXTURE_CUBE_MAP, sourceEnvCube);
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &envBaseSize);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        const float envResolution = static_cast<float>(std::max(envBaseSize, 1));

        glBindFramebuffer(GL_FRAMEBUFFER, mCaptureFbo);
        glUseProgram(mPrefilterShader->GetID());
        mPrefilterShader->SetMat4("uProjection", proj);
        mPrefilterShader->SetInt("uEnvironmentMap", 0);
        mPrefilterShader->SetFloat("uEnvMapResolution", envResolution);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, sourceEnvCube);

        glBindVertexArray(mCaptureMesh->GetVAO());

        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            const uint32_t mipSize = std::max(1u, baseSize >> mip);
            glBindRenderbuffer(GL_RENDERBUFFER, mCaptureRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(mipSize), static_cast<GLsizei>(mipSize));
            glViewport(0, 0, static_cast<GLsizei>(mipSize), static_cast<GLsizei>(mipSize));

            const float roughness = (mipLevels <= 1u) ? 0.0f : static_cast<float>(mip) / static_cast<float>(mipLevels - 1u);
            mPrefilterShader->SetFloat("uRoughness", roughness);

            for (uint32_t face = 0; face < 6u; ++face)
            {
                mPrefilterShader->SetMat4("uView", views[face]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, prefilterCube, static_cast<GLint>(mip));
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                if (mCaptureMesh->GetIndexCount() > 0u)
                    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mCaptureMesh->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
                else
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mCaptureMesh->GetVertexCount()));
            }
        }

        glBindVertexArray(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    bool IblPrecompute::ComputeBrdfLut(GLuint brdfLutTex, uint32_t width, uint32_t height)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mCaptureFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLutTex, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, mCaptureRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

        glUseProgram(mBrdfLutShader->GetID());
        glBindVertexArray(mFullscreenVao);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    bool IblPrecompute::SaveToDisk(const std::string &cacheKey, const RuntimeCacheEntry &entry)
    {
        const auto dir = CacheDirForKey(cacheKey);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        const auto faces = FaceShortNames();

        for (uint32_t face = 0; face < 6u; ++face)
        {
            std::vector<float> rgba{};
            if (!ReadCubemapFace(entry.mIrradiance, 0u, face, static_cast<int>(IrradianceSize), static_cast<int>(IrradianceSize), rgba))
                return false;

            ExrImage image{};
            image.mWidth = static_cast<int>(IrradianceSize);
            image.mHeight = static_cast<int>(IrradianceSize);
            image.mChannels = 4;
            image.mPixels = std::move(rgba);
            if (!SaveExrImage(dir / ("irradiance_" + std::string(faces[face]) + ".exr"), image))
                return false;
        }

        for (uint32_t mip = 0; mip < PrefilterMipLevels; ++mip)
        {
            const uint32_t size = std::max(1u, PrefilterBaseSize >> mip);
            for (uint32_t face = 0; face < 6u; ++face)
            {
                std::vector<float> rgba{};
                if (!ReadCubemapFace(entry.mPrefilter, mip, face, static_cast<int>(size), static_cast<int>(size), rgba))
                    return false;

                ExrImage image{};
                image.mWidth = static_cast<int>(size);
                image.mHeight = static_cast<int>(size);
                image.mChannels = 4;
                image.mPixels = std::move(rgba);
                if (!SaveExrImage(dir / ("prefilter_m" + std::to_string(mip) + "_" + std::string(faces[face]) + ".exr"), image))
                    return false;
            }
        }

        std::vector<float> rg{};
        if (!ReadBrdfLut(entry.mBrdfLut, static_cast<int>(BrdfLutSize), static_cast<int>(BrdfLutSize), rg))
            return false;

        ExrImage lut{};
        lut.mWidth = static_cast<int>(BrdfLutSize);
        lut.mHeight = static_cast<int>(BrdfLutSize);
        lut.mChannels = 4;
        lut.mPixels.assign(static_cast<size_t>(BrdfLutSize) * static_cast<size_t>(BrdfLutSize) * 4u, 1.0f);
        for (uint32_t y = 0; y < BrdfLutSize; ++y)
        {
            for (uint32_t x = 0; x < BrdfLutSize; ++x)
            {
                const size_t i2 = (static_cast<size_t>(y) * static_cast<size_t>(BrdfLutSize) + static_cast<size_t>(x)) * 2u;
                const size_t i4 = (static_cast<size_t>(y) * static_cast<size_t>(BrdfLutSize) + static_cast<size_t>(x)) * 4u;
                lut.mPixels[i4 + 0u] = rg[i2 + 0u];
                lut.mPixels[i4 + 1u] = rg[i2 + 1u];
                lut.mPixels[i4 + 2u] = 0.0f;
                lut.mPixels[i4 + 3u] = 1.0f;
            }
        }

        if (!SaveExrImage(dir / "brdf_lut_ggx_smith.exr", lut))
            return false;

        return true;
    }

    bool IblPrecompute::TryLoadFromDisk(const std::string &cacheKey, RuntimeCacheEntry &outEntry)
    {
        const auto dir = CacheDirForKey(cacheKey);
        if (!std::filesystem::exists(dir))
            return false;

        const auto faces = FaceShortNames();

        std::array<ExrImage, 6> irradianceFaces{};
        for (uint32_t face = 0; face < 6u; ++face)
        {
            if (!LoadExrImage(dir / ("irradiance_" + std::string(faces[face]) + ".exr"), irradianceFaces[face]))
                return false;
        }

        GLuint irradiance = CreateCubemapRGBA16F(static_cast<uint32_t>(irradianceFaces[0].mWidth), 1u, false);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance);
        for (uint32_t face = 0; face < 6u; ++face)
        {
            const ExrImage &img = irradianceFaces[face];
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                         0,
                         GL_RGBA16F,
                         img.mWidth,
                         img.mHeight,
                         0,
                         GL_RGBA,
                         GL_FLOAT,
                         img.mPixels.data());
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        GLuint prefilter = CreateCubemapRGBA16F(PrefilterBaseSize, PrefilterMipLevels, false);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter);
        for (uint32_t mip = 0; mip < PrefilterMipLevels; ++mip)
        {
            for (uint32_t face = 0; face < 6u; ++face)
            {
                ExrImage img{};
                if (!LoadExrImage(dir / ("prefilter_m" + std::to_string(mip) + "_" + std::string(faces[face]) + ".exr"), img))
                {
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                    return false;
                }

                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                             static_cast<GLint>(mip),
                             GL_RGBA16F,
                             img.mWidth,
                             img.mHeight,
                             0,
                             GL_RGBA,
                             GL_FLOAT,
                             img.mPixels.data());
            }
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        ExrImage lut{};
        if (!LoadExrImage(dir / "brdf_lut_ggx_smith.exr", lut))
            return false;

        GLuint brdf = CreateBrdfLutRG16F(static_cast<uint32_t>(lut.mWidth), static_cast<uint32_t>(lut.mHeight));
        std::vector<float> rg(static_cast<size_t>(lut.mWidth) * static_cast<size_t>(lut.mHeight) * 2u, 0.0f);
        for (int y = 0; y < lut.mHeight; ++y)
        {
            for (int x = 0; x < lut.mWidth; ++x)
            {
                const size_t i4 = (static_cast<size_t>(y) * static_cast<size_t>(lut.mWidth) + static_cast<size_t>(x)) * 4u;
                const size_t i2 = (static_cast<size_t>(y) * static_cast<size_t>(lut.mWidth) + static_cast<size_t>(x)) * 2u;
                rg[i2 + 0u] = lut.mPixels[i4 + 0u];
                rg[i2 + 1u] = lut.mPixels[i4 + 1u];
            }
        }

        glBindTexture(GL_TEXTURE_2D, brdf);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lut.mWidth, lut.mHeight, GL_RG, GL_FLOAT, rg.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        outEntry.mIrradiance = irradiance;
        outEntry.mPrefilter = prefilter;
        outEntry.mBrdfLut = brdf;
        return true;
    }

    bool IblPrecompute::Prepare(EnvironmentMap &env)
    {
        if (!env.IsValid())
            return false;

        if (!EnsureInit())
            return false;

        const std::string cacheKey = MakeCacheKey(env);

        if (const auto it = mRuntimeCache.find(cacheKey); it != mRuntimeCache.end())
        {
            env.SetIblMaps(it->second.mIrradiance, it->second.mPrefilter, it->second.mBrdfLut, cacheKey);
            return true;
        }

        RuntimeCacheEntry entry{};
        if (!TryLoadFromDisk(cacheKey, entry))
        {
            entry.mIrradiance = CreateCubemapRGBA16F(IrradianceSize, 1u, false);
            entry.mPrefilter = CreateCubemapRGBA16F(PrefilterBaseSize, PrefilterMipLevels, false);
            entry.mBrdfLut = CreateBrdfLutRG16F(BrdfLutSize, BrdfLutSize);

            if (entry.mIrradiance == 0u || entry.mPrefilter == 0u || entry.mBrdfLut == 0u)
                return false;

            if (!ComputeIrradiance(env.GetID(), entry.mIrradiance, IrradianceSize))
                return false;
            if (!ComputePrefilter(env.GetID(), entry.mPrefilter, PrefilterBaseSize, PrefilterMipLevels))
                return false;
            if (!ComputeBrdfLut(entry.mBrdfLut, BrdfLutSize, BrdfLutSize))
                return false;

            SaveToDisk(cacheKey, entry);
        }

        mRuntimeCache.emplace(cacheKey, entry);
        env.SetIblMaps(entry.mIrradiance, entry.mPrefilter, entry.mBrdfLut, cacheKey);
        return true;
    }
}
