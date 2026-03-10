#include "frameBuffer.hpp"
#include "utils/logger.hpp"
#include <utility>

namespace core
{
    void FrameBuffer::Release() noexcept
    {
        if (mDepthRBO != 0)
        {
            glDeleteRenderbuffers(1, &mDepthRBO);
            mDepthRBO = 0;
        }

        if (mColor != 0)
        {
            glDeleteTextures(1, &mColor);
            mColor = 0;
        }

        if (mFBO != 0)
        {
            glDeleteFramebuffers(1, &mFBO);
            mFBO = 0;
        }

        mWidth = 0;
        mHeight = 0;
    }

    FrameBuffer::~FrameBuffer()
    {
        Release();
    }

    FrameBuffer::FrameBuffer(FrameBuffer &&other) noexcept
    {
        *this = std::move(other);
    }

    FrameBuffer &FrameBuffer::operator=(FrameBuffer &&other) noexcept
    {
        if (this == &other)
            return *this;

        Release();

        mFBO = other.mFBO;
        mColor = other.mColor;
        mDepthRBO = other.mDepthRBO;
        mWidth = other.mWidth;
        mHeight = other.mHeight;
        mWithDepth = other.mWithDepth;

        other.mFBO = 0;
        other.mColor = 0;
        other.mDepthRBO = 0;
        other.mWidth = 0;
        other.mHeight = 0;
        return *this;
    }

    /*
     * 创建流程:
     * 1. 参数验证
     * 2. 清理现有资源
     * 3. 创建帧缓冲对象
     * 4. 创建颜色附件纹理
     * 5. 创建深度附件
     * 6. 完整性检查
     */
    bool FrameBuffer::Create(uint32_t width, uint32_t height, bool withDepth)
    {
        if (width == 0u || height == 0u)
        {
            GL_ERROR("[FrameBuffer] Create Failed: Invalid Size {}x{}", width, height);
            return false;
        }

        Release();

        mWidth = width;
        mHeight = height;
        mWithDepth = withDepth;

        // 创建帧缓冲对象
        glGenFramebuffers(1, &mFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

        // 创建颜色附件纹理
        glGenTextures(1, &mColor);
        glBindTexture(GL_TEXTURE_2D, mColor);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8, // 内部格式: 8位RGBA
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            GL_RGBA, // 像素格式: RGBA
            GL_UNSIGNED_BYTE,
            nullptr);

        // 设置纹理参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小过滤: 线性
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大过滤: 线性
        // 设置纹理环绕模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // S方向: 边缘钳制
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // T方向: 边缘钳制
        // 将纹理附加到帧缓冲的颜色附件0
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor, 0);

        if (mWithDepth)
        {
            glGenRenderbuffers(1, &mDepthRBO);
            glBindRenderbuffer(GL_RENDERBUFFER, mDepthRBO); // 绑定渲染缓冲

            // 分配渲染缓冲存储, 24位深度 + 8位模板
            glRenderbufferStorage(
                GL_RENDERBUFFER,
                GL_DEPTH24_STENCIL8,
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height));

            // 将渲染缓冲附加到帧缓冲的深度模板附件
            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                GL_DEPTH_STENCIL_ATTACHMENT,
                GL_RENDERBUFFER,
                mDepthRBO);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); // 检查帧缓冲完整性
        glBindFramebuffer(GL_FRAMEBUFFER, 0);                           // 解绑帧缓冲(恢复默认)

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            GL_ERROR("[FrameBuffer] Create Failed: Status={}", static_cast<uint32_t>(status));
            Release();
            return false;
        }

        return true;
    }

    void FrameBuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width == mWidth && height == mHeight) // 尺寸相同检查
            return;

        Create(width, height, mWithDepth); // 重新创建
    }

    void FrameBuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO); // 绑定该帧缓冲
        glViewport(0, 0, static_cast<GLsizei>(mWidth), static_cast<GLsizei>(mHeight));
    }

    void FrameBuffer::BindDefault()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}