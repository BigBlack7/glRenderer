#pragma once
#include <glad/glad.h>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <array>
#include <memory>

namespace core
{
    class Texture final
    {
    private:
        GLuint mTextureID{0};
        uint32_t mWidth{0};
        uint32_t mHeight{0};
        uint32_t mUnit{0};
        std::string mDebugName{};
        GLenum mTarget{GL_TEXTURE_2D};

    public:
        struct CreateInfo final
        {
            bool __flipY__{true};
            bool __sRGB__{false}; // 是否使用sRGB空间(gamma矫正)
            GLint __wrapS__{GL_REPEAT};
            GLint __wrapT__{GL_REPEAT};
            GLint __magFilter__{GL_LINEAR};
            GLint __minFilter__{GL_LINEAR_MIPMAP_LINEAR};
        };

    private:
        Texture() = default;

        /// @brief 释放纹理资源
        void Release() noexcept;

        /// @brief 上传RGBA8格式像素数据到GPU
        /// @param pixels
        /// @param width
        /// @param height
        /// @param info
        /// @return
        bool UploadRGBA8(const unsigned char *pixels, uint32_t width, uint32_t height, const CreateInfo &info) noexcept;

        /// @brief 上传CubeMap RGBA8格式像素数据到GPU
        /// @param facePixels - 6个面的像素数据，顺序为 +X(right), -X(left), +Y(top), -Y(bottom), +Z(back), -Z(front)
        /// @param width 每个面的宽度
        /// @param height 每个面的高度
        /// @return 是否上传成功
        bool UploadCubemapRGBA8(const std::array<const unsigned char *, 6> &facePixels, uint32_t width, uint32_t height) noexcept;

        /// @brief 判断是否需要生成Mipmap
        /// @param minFilter 最小过滤参数
        /// @return 是否需要生成Mipmap
        static bool NeedsMipmap(GLint minFilter) noexcept;

        /// @brief 解析纹理文件路径
        /// @param path 纹理文件路径
        /// @param baseDir 基础目录
        /// @return 解析后的绝对路径
        static std::filesystem::path ResolveTexturePath(const std::filesystem::path &path, const std::filesystem::path &baseDir);

    public:
        Texture(const std::string &path, uint32_t unit);                         // 兼容层
        Texture(const std::string &path, uint32_t unit, const CreateInfo &info); // 兼容层(可指定sRGB等参数)

        /// @brief 支持"模型目录 + 相对路径"的文件纹理加载(从文件加载纹理)
        /// @param path 纹理文件路径
        /// @param unit 纹理单元
        /// @param baseDir 基础目录
        /// @param info 创建信息
        Texture(const std::filesystem::path &path,
                uint32_t unit,
                const std::filesystem::path &baseDir,
                const CreateInfo &info = {});

        /// @brief 支持Assimp内嵌压缩纹理加载(从内存数据加载纹理)
        /// @param encodedBytes 压缩纹理数据
        /// @param unit 纹理单元
        /// @param debugName 调试名称
        /// @param info 创建信息
        Texture(std::span<const uint8_t> encodedBytes,
                uint32_t unit,
                std::string debugName = {},
                const CreateInfo &info = {});

        /// @brief 支持Assimp内嵌未压缩RGBA纹理加载(从原始RGBA像素数据创建纹理)
        /// @param rgbaPixels RGBA格式的像素数据
        /// @param width 纹理宽度
        /// @param height 纹理高度
        /// @param unit 纹理单元
        /// @param debugName 调试名称
        /// @param info 创建信息
        Texture(const uint8_t *rgbaPixels,
                uint32_t width,
                uint32_t height,
                uint32_t unit,
                std::string debugName = {},
                const CreateInfo &info = {});

        ~Texture();

        // 禁止拷贝
        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;

        // 允许移动
        Texture(Texture &&other) noexcept;
        Texture &operator=(Texture &&other) noexcept;

        bool IsValid() const noexcept { return mTextureID != 0; }

        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }
        GLuint GetID() const { return mTextureID; }
        uint32_t GetUnit() const { return mUnit; }
        GLenum GetTarget() const noexcept { return mTarget; }
        const std::string &GetDebugName() const noexcept { return mDebugName; }

        // Environment Map
        static std::shared_ptr<Texture> CreatePanorama(const std::filesystem::path &path, const std::filesystem::path &baseDir = {});
        static std::shared_ptr<Texture> CreateCubemap(const std::array<std::filesystem::path, 6> &cubeFacePaths, const std::filesystem::path &baseDir = {});
    };
}