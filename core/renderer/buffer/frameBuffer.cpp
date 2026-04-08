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

        if (mColorMSAARBO != 0)
        {
            glDeleteRenderbuffers(1, &mColorMSAARBO);
            mColorMSAARBO = 0;
        }

        if (mColor != 0)
        {
            glDeleteTextures(1, &mColor);
            mColor = 0;
        }

        if (mResolveFBO != 0)
        {
            glDeleteFramebuffers(1, &mResolveFBO);
            mResolveFBO = 0;
        }

        if (mFBO != 0)
        {
            glDeleteFramebuffers(1, &mFBO);
            mFBO = 0;
        }

        mWidth = 0;
        mHeight = 0;
        mSamples = 1u;
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
    bool FrameBuffer::Create(uint32_t width, uint32_t height, bool withDepth, uint32_t samples)
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
        mSamples = std::max(samples, 1u);

        const bool useMSAA = mSamples > 1u;

        // 创建帧缓冲对象
        glGenFramebuffers(1, &mFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

        // 创建可采样颜色附件纹理(后处理采样)
        glGenTextures(1, &mColor);
        glBindTexture(GL_TEXTURE_2D, mColor);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA16F, // HDR颜色缓冲
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            GL_RGBA, // 像素格式: RGBA
            GL_FLOAT,
            nullptr);

        // 设置纹理参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小过滤: 线性
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大过滤: 线性
        // 设置纹理环绕模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // S方向: 边缘钳制
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // T方向: 边缘钳制

        if (useMSAA)
        {
            // 主FBO绑定MSAA颜色RenderBuffer
            glGenRenderbuffers(1, &mColorMSAARBO);
            glBindRenderbuffer(GL_RENDERBUFFER, mColorMSAARBO);
            glRenderbufferStorageMultisample(
                GL_RENDERBUFFER,
                static_cast<GLsizei>(mSamples),
                GL_RGBA16F,
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height));
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mColorMSAARBO);

            // 创建resolve FBO并挂可采样颜色纹理
            glGenFramebuffers(1, &mResolveFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, mResolveFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor, 0);
        }
        else
        {
            // 非MSAA直接把纹理挂在主FBO
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor, 0);
        }

        if (mWithDepth)
        {
            glGenRenderbuffers(1, &mDepthRBO);
            glBindRenderbuffer(GL_RENDERBUFFER, mDepthRBO); // 绑定渲染缓冲

            if (useMSAA)
            {
                // 分配MSAA深度模板缓冲
                glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER,
                    static_cast<GLsizei>(mSamples),
                    GL_DEPTH24_STENCIL8,
                    static_cast<GLsizei>(width),
                    static_cast<GLsizei>(height));
            }
            else
            {
                // 分配单采样深度模板缓冲
                glRenderbufferStorage(
                    GL_RENDERBUFFER,
                    GL_DEPTH24_STENCIL8,
                    static_cast<GLsizei>(width),
                    static_cast<GLsizei>(height));
            }

            // 将渲染缓冲附加到帧缓冲的深度模板附件
            glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                GL_DEPTH_STENCIL_ATTACHMENT,
                GL_RENDERBUFFER,
                mDepthRBO);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); // 检查主帧缓冲完整性

        GLenum resolveStatus = GL_FRAMEBUFFER_COMPLETE;
        if (useMSAA)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mResolveFBO);
            resolveStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER); // 检查解析帧缓冲完整性
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0); // 解绑帧缓冲(恢复默认)

        if (status != GL_FRAMEBUFFER_COMPLETE || resolveStatus != GL_FRAMEBUFFER_COMPLETE)
        {
            GL_ERROR("[FrameBuffer] Create Failed: MainStatus={} ResolveStatus={}", static_cast<uint32_t>(status), static_cast<uint32_t>(resolveStatus));
            Release();
            return false;
        }

        return true;
    }

    void FrameBuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width == mWidth && height == mHeight) // 尺寸相同检查
            return;

        Create(width, height, mWithDepth, mSamples); // 重新创建
    }

    void FrameBuffer::ResolveColor() const
    {
        if (!IsValid() || mSamples <= 1u || mResolveFBO == 0)
            return;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mResolveFBO);
        glBlitFramebuffer(
            0,
            0,
            static_cast<GLint>(mWidth),
            static_cast<GLint>(mHeight),
            0,
            0,
            static_cast<GLint>(mWidth),
            static_cast<GLint>(mHeight),
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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