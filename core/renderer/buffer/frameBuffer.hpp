#pragma once
#include <glad/glad.h>
#include <cstdint>

namespace core
{
    class FrameBuffer final
    {
    private:
        GLuint mFBO{0};      // 帧缓冲对象ID
        GLuint mColor{0};    // 颜色附件纹理ID
        GLuint mDepthRBO{0}; // 深度缓冲ID

        uint32_t mWidth{0};
        uint32_t mHeight{0};
        bool mWithDepth{true}; // 是否包含深度缓冲

        /// @brief 释放所有OpenGL资源
        void Release() noexcept;

    public:
        FrameBuffer() = default;
        ~FrameBuffer();

        // 禁止拷贝(OpenGL对象不应被拷贝)
        FrameBuffer(const FrameBuffer &) = delete;
        FrameBuffer &operator=(const FrameBuffer &) = delete;

        // 允许移动(支持RAII资源转移)
        FrameBuffer(FrameBuffer &&other) noexcept;
        FrameBuffer &operator=(FrameBuffer &&other) noexcept;

        /// @brief 创建帧缓冲对象
        /// @param width 帧缓冲宽度
        /// @param height 帧缓冲高度
        /// @param withDepth 是否包含深度缓冲
        /// @return 创建成功返回true
        bool Create(uint32_t width, uint32_t height, bool withDepth = true);

        /// @brief 调整帧缓冲大小
        /// @param width 帧缓冲新宽度
        /// @param height 帧缓冲新高度
        void Resize(uint32_t width, uint32_t height);

        /// @brief 绑定此帧缓冲为当前渲染目标
        void Bind() const;

        /// @brief 绑定默认帧缓冲(屏幕缓冲)(用于恢复默认渲染目标)
        static void BindDefault();

        bool IsValid() const noexcept { return mFBO != 0; }
        GLuint GetColorAttachment() const noexcept { return mColor; }
        uint32_t GetWidth() const noexcept { return mWidth; }
        uint32_t GetHeight() const noexcept { return mHeight; }
    };
}