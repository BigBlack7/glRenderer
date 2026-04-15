#include "texture.hpp"
#include "exrIO.hpp"
#include "utils/logger.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>

namespace core
{
    namespace detail
    {
        const std::filesystem::path TextureRoot = "../../../assets/texture/";
        const std::filesystem::path EnvRoot = "../../../assets/env/";

        constexpr float Pi = 3.14159265359f;

        glm::vec3 CubemapDirectionFromFaceUV(size_t faceIndex, float u, float v)
        {
            switch (faceIndex)
            {
            case 0: // +X
                return glm::normalize(glm::vec3(1.0f, -v, -u));
            case 1: // -X
                return glm::normalize(glm::vec3(-1.0f, -v, u));
            case 2: // +Y
                return glm::normalize(glm::vec3(u, 1.0f, v));
            case 3: // -Y
                return glm::normalize(glm::vec3(u, -1.0f, -v));
            case 4: // +Z
                return glm::normalize(glm::vec3(u, -v, 1.0f));
            default: // 5 -Z
                return glm::normalize(glm::vec3(-u, -v, -1.0f));
            }
        }

        glm::vec2 DirectionToEquirectUV(const glm::vec3 &dir)
        {
            const glm::vec3 n = glm::normalize(dir);
            const float u = std::atan2(n.z, n.x) / (2.0f * Pi) + 0.5f;
            const float v = std::asin(glm::clamp(n.y, -1.0f, 1.0f)) / Pi + 0.5f;
            return glm::vec2(u, v);
        }

        glm::vec4 SampleEquirectBilinear(const std::vector<float> &pixels, int width, int height, const glm::vec2 &uv)
        {
            const float fu = glm::fract(uv.x);
            const float fv = glm::clamp(uv.y, 0.0f, 1.0f);

            const float x = fu * static_cast<float>(width - 1);
            const float y = fv * static_cast<float>(height - 1);

            const int x0 = static_cast<int>(std::floor(x));
            const int y0 = static_cast<int>(std::floor(y));
            const int x1 = (x0 + 1) % width;
            const int y1 = std::min(y0 + 1, height - 1);

            const float tx = x - static_cast<float>(x0);
            const float ty = y - static_cast<float>(y0);

            auto Load = [&](int ix, int iy) -> glm::vec4
            {
                const size_t i = (static_cast<size_t>(iy) * static_cast<size_t>(width) + static_cast<size_t>(ix)) * 4u;
                return glm::vec4(pixels[i + 0u], pixels[i + 1u], pixels[i + 2u], pixels[i + 3u]);
            };

            const glm::vec4 c00 = Load(x0, y0);
            const glm::vec4 c10 = Load(x1, y0);
            const glm::vec4 c01 = Load(x0, y1);
            const glm::vec4 c11 = Load(x1, y1);

            const glm::vec4 c0 = glm::mix(c00, c10, tx);
            const glm::vec4 c1 = glm::mix(c01, c11, tx);
            return glm::mix(c0, c1, ty);
        }
    }

    bool Texture::UploadRGBA16F(const float *pixels, uint32_t width, uint32_t height, const CreateInfo &info) noexcept
    {
        if (!pixels || width == 0 || height == 0)
            return false;

        if (mTextureID == 0)
            glGenTextures(1, &mTextureID);

        glBindTexture(GL_TEXTURE_2D, mTextureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA16F,
                     static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height),
                     0,
                     GL_RGBA,
                     GL_FLOAT,
                     pixels);

        mTarget = GL_TEXTURE_2D;
        if (NeedsMipmap(info.__minFilter__))
            glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, info.__wrapS__);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, info.__wrapT__);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, info.__magFilter__);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, info.__minFilter__);
        glBindTexture(GL_TEXTURE_2D, 0);

        mWidth = width;
        mHeight = height;
        return true;
    }

    bool Texture::UploadCubemapRGBA16F(const std::array<const float *, 6> &facePixels, uint32_t width, uint32_t height) noexcept
    {
        if (width == 0 || height == 0)
            return false;

        for (const auto *p : facePixels)
        {
            if (!p)
                return false;
        }

        if (mTextureID == 0)
            glGenTextures(1, &mTextureID);

        mTarget = GL_TEXTURE_CUBE_MAP;
        glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (size_t i = 0; i < facePixels.size(); ++i)
        {
            glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i),
                         0,
                         GL_RGBA16F,
                         static_cast<GLsizei>(width),
                         static_cast<GLsizei>(height),
                         0,
                         GL_RGBA,
                         GL_FLOAT,
                         facePixels[i]);
        }

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        mWidth = width;
        mHeight = height;
        return true;
    }

    void Texture::Release() noexcept
    {
        if (mTextureID != 0)
        {
            glDeleteTextures(1, &mTextureID);
            mTextureID = 0;
        }

        mWidth = 0;
        mHeight = 0;
        mUnit = 0;
        mTarget = GL_TEXTURE_2D;
    }

    bool Texture::UploadRGBA8(const unsigned char *pixels, uint32_t width, uint32_t height, const CreateInfo &info) noexcept
    {
        if (!pixels || width == 0 || height == 0)
            return false;

        if (mTextureID == 0)
            glGenTextures(1, &mTextureID);

        glBindTexture(GL_TEXTURE_2D, mTextureID); // 绑定纹理对象
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);    // 设置像素对齐方式为1字节

        const GLint internalFormat = info.__sRGB__ ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        glTexImage2D(GL_TEXTURE_2D,                // 上传纹理数据
                     0,                            // mipmap级别0
                     internalFormat,               // 内部格式: sRGB或线性RGBA8
                     static_cast<GLsizei>(width),  // 纹理宽度
                     static_cast<GLsizei>(height), // 纹理高度
                     0,                            // 边框宽度, 必须为0
                     GL_RGBA,                      // 输入像素格式
                     GL_UNSIGNED_BYTE,             // 输入像素数据类型
                     pixels);                      // 像素数据指针

        mTarget = GL_TEXTURE_2D;
        if (NeedsMipmap(info.__minFilter__))
            glGenerateMipmap(GL_TEXTURE_2D); // 生成mipmap链

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, info.__wrapS__);         // S方向环绕模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, info.__wrapT__);         // T方向环绕模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, info.__magFilter__); // 放大
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, info.__minFilter__); // 缩小过滤
        glBindTexture(GL_TEXTURE_2D, 0);                                           // 解绑纹理

        mWidth = width;
        mHeight = height;
        return true;
    }

    bool Texture::UploadCubemapRGBA8(const std::array<const unsigned char *, 6> &facePixels, uint32_t width, uint32_t height) noexcept
    {
        if (width == 0 || height == 0)
            return false;

        for (const auto *p : facePixels) // 检查每个面的像素数据是否为空
        {
            if (!p)
                return false;
        }

        if (mTextureID == 0)
            glGenTextures(1, &mTextureID);

        mTarget = GL_TEXTURE_CUBE_MAP;

        glBindTexture(GL_TEXTURE_CUBE_MAP, mTextureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (size_t i = 0; i < facePixels.size(); ++i)
        {
            glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i),
                         0,
                         GL_RGBA8,
                         static_cast<GLsizei>(width),
                         static_cast<GLsizei>(height),
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         facePixels[i]);
        }

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        // 固定最佳状态
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        mWidth = width;
        mHeight = height;
        return true;
    }

    bool Texture::NeedsMipmap(GLint minFilter) noexcept
    {
        return minFilter == GL_NEAREST_MIPMAP_NEAREST ||
               minFilter == GL_LINEAR_MIPMAP_NEAREST ||
               minFilter == GL_NEAREST_MIPMAP_LINEAR ||
               minFilter == GL_LINEAR_MIPMAP_LINEAR;
    }

    std::filesystem::path Texture::ResolveTexturePath(const std::filesystem::path &path, const std::filesystem::path &baseDir)
    {
        if (path.empty()) // 空路径返回
            return path;

        if (path.is_absolute() && std::filesystem::exists(path)) // 如果是绝对路径且文件存在
            return path;

        if (!baseDir.empty()) // 如果基础目录不为空
        {
            const std::filesystem::path p0 = baseDir / path; // 尝试基础目录+相对路径构造绝对路径
            if (std::filesystem::exists(p0))                 // 该路径存在文件
                return p0;

            const std::filesystem::path p1 = baseDir / path.filename(); // 尝试基础目录+文件名
            if (std::filesystem::exists(p1))
                return p1;
        }

        if (std::filesystem::exists(path)) // 如果相对路径在当前目录存在
            return path;

        const std::string ext = path.extension().string();
        const bool isExr = (ext == ".exr" || ext == ".EXR");
        if (isExr)
        {
            const std::filesystem::path pEnv0 = detail::EnvRoot / path;
            if (std::filesystem::exists(pEnv0))
                return pEnv0;

            const std::filesystem::path pEnv1 = detail::EnvRoot / path.filename();
            if (std::filesystem::exists(pEnv1))
                return pEnv1;
        }

        const std::filesystem::path p2 = detail::TextureRoot / path; // 尝试资源根目录+相对路径
        if (std::filesystem::exists(p2))
            return p2;

        const std::filesystem::path p3 = detail::TextureRoot / path.filename(); // 尝试资源根目录+文件名
        if (std::filesystem::exists(p3))
            return p3;

        return path;
    }

    Texture::Texture(const std::string &path, uint32_t unit) : Texture(std::filesystem::path(path), unit, {}) {}

    Texture::Texture(const std::string &path, uint32_t unit, const CreateInfo &info) : Texture(std::filesystem::path(path), unit, {}, info) {}

    Texture::Texture(const std::filesystem::path &path,
                     uint32_t unit,
                     const std::filesystem::path &baseDir,
                     const CreateInfo &info)
        : mUnit(unit)
    {
        const std::filesystem::path resolvedPath = ResolveTexturePath(path, baseDir);
        mDebugName = resolvedPath.generic_string();

        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_set_flip_vertically_on_load(info.__flipY__ ? 1 : 0);        // 设置Y轴翻转
        unsigned char *pixels = stbi_load(resolvedPath.string().c_str(), // 加载图像文件
                                          &width,
                                          &height,
                                          &channels,
                                          STBI_rgb_alpha); // 强制转换为RGBA格式
        stbi_set_flip_vertically_on_load(0);               // 重置Y轴翻转设置

        if (!pixels)
        {
            GL_ERROR("[Texture] File Load Failed: {} | {}", mDebugName, stbi_failure_reason() ? stbi_failure_reason() : "Unknown");
            return;
        }

        if (!UploadRGBA8(pixels, // 上传像素数据到GPU
                         static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height),
                         info))
        {
            GL_ERROR("[Texture] Upload Failed: {}", mDebugName);
        }

        stbi_image_free(pixels); // 释放stb_image分配的像素内存
    }

    Texture::Texture(std::span<const uint8_t> encodedBytes,
                     uint32_t unit,
                     std::string debugName,
                     const CreateInfo &info)
        : mUnit(unit),
          mDebugName(std::move(debugName))
    {
        if (encodedBytes.empty())
        {
            GL_ERROR("[Texture] Encoded Bytes Are Empty");
            return;
        }

        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_set_flip_vertically_on_load(info.__flipY__ ? 1 : 0);
        unsigned char *pixels = stbi_load_from_memory(encodedBytes.data(), // 从内存加载图像
                                                      static_cast<int>(encodedBytes.size()),
                                                      &width,
                                                      &height,
                                                      &channels,
                                                      STBI_rgb_alpha);
        stbi_set_flip_vertically_on_load(0);

        if (!pixels)
        {
            GL_ERROR("[Texture] Memory Decode Failed: {} | {}", mDebugName, stbi_failure_reason() ? stbi_failure_reason() : "Unknown");
            return;
        }

        if (!UploadRGBA8(pixels,
                         static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height),
                         info))
        {
            GL_ERROR("[Texture] Upload Failed: {}", mDebugName);
        }

        stbi_image_free(pixels);
    }

    Texture::Texture(const uint8_t *rgbaPixels,
                     uint32_t width,
                     uint32_t height,
                     uint32_t unit,
                     std::string debugName,
                     const CreateInfo &info)
        : mUnit(unit),
          mDebugName(std::move(debugName))
    {
        if (!rgbaPixels || width == 0 || height == 0)
        {
            GL_ERROR("[Texture] Raw RGBA Input Invalid: {}", mDebugName);
            return;
        }

        if (!info.__flipY__) // 不需要Y轴翻转直接上传数据
        {
            UploadRGBA8(rgbaPixels, width, height, info);
            return;
        }
        // 需要Y轴翻转时的处理
        const size_t rowBytes = static_cast<size_t>(width) * 4u;                    // 每行字节数(RGBA 4字节)
        std::vector<uint8_t> flipped(static_cast<size_t>(height) * rowBytes, 255u); // 创建翻转后的数据缓冲区
        for (uint32_t y = 0; y < height; ++y)                                       // 逐行翻转
        {
            const uint32_t srcY = height - 1u - y;                                                                                        // 源行索引(从底部开始)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * rowBytes, rgbaPixels + static_cast<size_t>(srcY) * rowBytes, rowBytes); // 复制一行数据
        }

        UploadRGBA8(flipped.data(), width, height, info);
    }

    Texture::Texture(Texture &&other) noexcept
    {
        *this = std::move(other);
    }

    Texture &Texture::operator=(Texture &&other) noexcept
    {
        if (this == &other) // 自我赋值检查
            return *this;

        Release();

        mTextureID = std::exchange(other.mTextureID, 0u); // 转移纹理ID, 源对象重置为0
        mWidth = std::exchange(other.mWidth, 0u);
        mHeight = std::exchange(other.mHeight, 0u);
        mUnit = std::exchange(other.mUnit, 0u);
        mDebugName = std::move(other.mDebugName);

        return *this;
    }

    Texture::~Texture()
    {
        Release();
    }

    std::shared_ptr<Texture> Texture::CreatePanorama(const std::filesystem::path &path, const std::filesystem::path &baseDir)
    {
        CreateInfo info{};
        info.__flipY__ = true;
        info.__wrapS__ = GL_REPEAT;
        info.__wrapT__ = GL_CLAMP_TO_EDGE;
        info.__magFilter__ = GL_LINEAR;
        info.__minFilter__ = GL_LINEAR;

        const std::filesystem::path resolved = ResolveTexturePath(path, baseDir);
        const std::string ext = resolved.extension().string();
        if (ext == ".exr" || ext == ".EXR")
        {
            ExrImage image{};
            if (!LoadExrImage(resolved, image))
                return nullptr;

            auto tex = std::shared_ptr<Texture>(new Texture());
            tex->mUnit = 0u;
            tex->mDebugName = resolved.generic_string();
            if (!tex->UploadRGBA16F(image.mPixels.data(), static_cast<uint32_t>(image.mWidth), static_cast<uint32_t>(image.mHeight), info))
                return nullptr;
            return tex;
        }

        auto tex = std::make_shared<Texture>(resolved, 0u, std::filesystem::path{}, info);
        return tex->IsValid() ? tex : nullptr;
    }

    std::shared_ptr<Texture> Texture::CreateCubemap(const std::array<std::filesystem::path, 6> &cubeFacePaths, const std::filesystem::path &baseDir)
    {
        std::array<unsigned char *, 6> pixels{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        int width = 0;
        int height = 0;
        bool ok = true;
        std::string debugName{};

        auto freeFaces = [&pixels]()
        {
            for (auto *p : pixels)
            {
                if (p)
                    stbi_image_free(p);
            }
        };

        stbi_set_flip_vertically_on_load(0);

        for (size_t i = 0; i < cubeFacePaths.size(); ++i)
        {
            const std::filesystem::path resolved = ResolveTexturePath(cubeFacePaths[i], baseDir);
            if (i == 0)
                debugName = resolved.parent_path().generic_string();

            int w = 0;
            int h = 0;
            int c = 0;
            pixels[i] = stbi_load(resolved.string().c_str(), &w, &h, &c, STBI_rgb_alpha);

            if (!pixels[i])
            {
                GL_ERROR("[Texture] Cubemap Face Load Failed: {} | {}",
                         resolved.string(),
                         stbi_failure_reason() ? stbi_failure_reason() : "Unknown");
                ok = false;
                break;
            }

            if (i == 0)
            {
                width = w;
                height = h;
            }
            else if (w != width || h != height)
            {
                GL_ERROR("[Texture] Cubemap Face Size Mismatch: {} ({}x{}), Expected ({}x{})",
                         resolved.string(), w, h, width, height);
                ok = false;
                break;
            }
        }

        if (!ok || width <= 0 || height <= 0)
        {
            freeFaces();
            return nullptr;
        }

        auto tex = std::shared_ptr<Texture>(new Texture());
        tex->mUnit = 0u;
        tex->mDebugName = std::move(debugName);

        std::array<const unsigned char *, 6> facePtrs{pixels[0], pixels[1], pixels[2], pixels[3], pixels[4], pixels[5]};

        if (!tex->UploadCubemapRGBA8(facePtrs, static_cast<uint32_t>(width), static_cast<uint32_t>(height)))
        {
            GL_ERROR("[Texture] Cubemap Upload Failed: {}", tex->mDebugName);
            freeFaces();
            return nullptr;
        }

        freeFaces();
        return tex;
    }

    std::shared_ptr<Texture> Texture::CreateCubemapFromPanorama(const std::filesystem::path &panoramaPath,
                                                                const std::filesystem::path &baseDir,
                                                                uint32_t cubeSize)
    {
        const std::filesystem::path resolved = ResolveTexturePath(panoramaPath, baseDir);
        std::vector<float> panoPixels{};
        int panoW = 0;
        int panoH = 0;

        const std::string ext = resolved.extension().string();
        if (ext == ".exr" || ext == ".EXR")
        {
            ExrImage image{};
            if (!LoadExrImage(resolved, image))
                return nullptr;

            panoW = image.mWidth;
            panoH = image.mHeight;
            if (image.mChannels == 4)
            {
                panoPixels = std::move(image.mPixels);
            }
            else
            {
                panoPixels.resize(static_cast<size_t>(panoW) * static_cast<size_t>(panoH) * 4u, 1.0f);
                for (int y = 0; y < panoH; ++y)
                {
                    for (int x = 0; x < panoW; ++x)
                    {
                        const size_t i3 = (static_cast<size_t>(y) * static_cast<size_t>(panoW) + static_cast<size_t>(x)) * 3u;
                        const size_t i4 = (static_cast<size_t>(y) * static_cast<size_t>(panoW) + static_cast<size_t>(x)) * 4u;
                        panoPixels[i4 + 0u] = image.mPixels[i3 + 0u];
                        panoPixels[i4 + 1u] = image.mPixels[i3 + 1u];
                        panoPixels[i4 + 2u] = image.mPixels[i3 + 2u];
                        panoPixels[i4 + 3u] = 1.0f;
                    }
                }
            }
        }
        else
        {
            int channels = 0;
            stbi_set_flip_vertically_on_load(1);
            float *pixels = stbi_loadf(resolved.string().c_str(), &panoW, &panoH, &channels, 4);
            stbi_set_flip_vertically_on_load(0);
            if (!pixels)
            {
                GL_ERROR("[Texture] Panorama Load Failed: {} | {}", resolved.string(), stbi_failure_reason() ? stbi_failure_reason() : "Unknown");
                return nullptr;
            }

            panoPixels.assign(pixels, pixels + static_cast<size_t>(panoW) * static_cast<size_t>(panoH) * 4u);
            stbi_image_free(pixels);
        }

        if (panoW <= 0 || panoH <= 0 || panoPixels.empty())
            return nullptr;

        const uint32_t size = std::max(cubeSize, 16u);
        std::array<std::vector<float>, 6> faceData{};
        for (auto &face : faceData)
            face.resize(static_cast<size_t>(size) * static_cast<size_t>(size) * 4u, 1.0f);

        for (size_t face = 0; face < 6u; ++face)
        {
            for (uint32_t y = 0; y < size; ++y)
            {
                for (uint32_t x = 0; x < size; ++x)
                {
                    const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
                    const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;
                    const glm::vec3 dir = detail::CubemapDirectionFromFaceUV(face, u, v);
                    const glm::vec2 uv = detail::DirectionToEquirectUV(dir);
                    const glm::vec4 c = detail::SampleEquirectBilinear(panoPixels, panoW, panoH, uv);

                    const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4u;
                    faceData[face][i + 0u] = c.r;
                    faceData[face][i + 1u] = c.g;
                    faceData[face][i + 2u] = c.b;
                    faceData[face][i + 3u] = c.a;
                }
            }
        }

        auto tex = std::shared_ptr<Texture>(new Texture());
        tex->mUnit = 0u;
        tex->mDebugName = resolved.generic_string();

        std::array<const float *, 6> facePtrs{
            faceData[0].data(),
            faceData[1].data(),
            faceData[2].data(),
            faceData[3].data(),
            faceData[4].data(),
            faceData[5].data()};

        if (!tex->UploadCubemapRGBA16F(facePtrs, size, size))
        {
            GL_ERROR("[Texture] Panorama->Cubemap Upload Failed: {}", resolved.string());
            return nullptr;
        }

        return tex;
    }
}