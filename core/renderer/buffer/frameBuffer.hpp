#pragma once
#include <glad/glad.h>
#include <cstdint>

namespace core
{
    class FrameBuffer final
    {
    private:
        GLuint mFBO{0};          // 主渲染帧缓冲对象ID
        GLuint mResolveFBO{0};   // 解析帧缓冲对象ID(MSAA时用于blit)
        GLuint mColor{0};        // 可采样颜色附件纹理ID(后处理采样源)
        GLuint mColorMSAARBO{0}; // MSAA颜色缓冲(MSAA时使用)
        GLuint mDepthRBO{0};     // 深度缓冲ID

        uint32_t mWidth{0};
        uint32_t mHeight{0};
        bool mWithDepth{true}; // 是否包含深度缓冲
        uint32_t mSamples{1};  // MSAA采样数(1表示关闭MSAA)

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
        /// @param samples MSAA采样数(1表示关闭MSAA)
        /// @return 创建成功返回true
        bool Create(uint32_t width, uint32_t height, bool withDepth = true, uint32_t samples = 1u);

        /// @brief 调整帧缓冲大小
        /// @param width 帧缓冲新宽度
        /// @param height 帧缓冲新高度
        void Resize(uint32_t width, uint32_t height);

        /// @brief 将MSAA颜色缓冲解析到可采样颜色纹理; 非MSAA时为no-op
        void ResolveColor() const;

        /// @brief 绑定此帧缓冲为当前渲染目标
        void Bind() const;

        /// @brief 绑定默认帧缓冲(屏幕缓冲)(用于恢复默认渲染目标)
        static void BindDefault();

        bool IsValid() const noexcept { return mFBO != 0; }
        GLuint GetColorAttachment() const noexcept { return mColor; }
        uint32_t GetWidth() const noexcept { return mWidth; }
        uint32_t GetHeight() const noexcept { return mHeight; }
        uint32_t GetSamples() const noexcept { return mSamples; }
        bool IsMultisampled() const noexcept { return mSamples > 1u; }
    };
}