#include "frameBuffer.hpp"
#include "utils/logger.hpp"
#include <utility>

namespace core
{
    void FrameBuffer::Release() noexcept
    {
        if (mDepthRbo != 0)
        {
            glDeleteRenderbuffers(1, &mDepthRbo);
            mDepthRbo = 0;
        }

        if (mColor != 0)
        {
            glDeleteTextures(1, &mColor);
            mColor = 0;
        }

        if (mFbo != 0)
        {
            glDeleteFramebuffers(1, &mFbo);
            mFbo = 0;
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

        mFbo = other.mFbo;
        mColor = other.mColor;
        mDepthRbo = other.mDepthRbo;
        mWidth = other.mWidth;
        mHeight = other.mHeight;
        mWithDepth = other.mWithDepth;

        other.mFbo = 0;
        other.mColor = 0;
        other.mDepthRbo = 0;
        other.mWidth = 0;
        other.mHeight = 0;
        return *this;
    }

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

        glGenFramebuffers(1, &mFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mFbo);

        glGenTextures(1, &mColor);
        glBindTexture(GL_TEXTURE_2D, mColor);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColor, 0);

        if (mWithDepth)
        {
            glGenRenderbuffers(1, &mDepthRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, mDepthRbo);
            glRenderbufferStorage(
                GL_RENDERBUFFER,
                GL_DEPTH24_STENCIL8,
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height));
            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                GL_DEPTH_STENCIL_ATTACHMENT,
                GL_RENDERBUFFER,
                mDepthRbo);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            GL_ERROR("[FrameBuffer] Create Failed: status={}", static_cast<uint32_t>(status));
            Release();
            return false;
        }

        return true;
    }

    void FrameBuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width == mWidth && height == mHeight)
            return;

        Create(width, height, mWithDepth);
    }

    void FrameBuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
        glViewport(0, 0, static_cast<GLsizei>(mWidth), static_cast<GLsizei>(mHeight));
    }

    void FrameBuffer::BindDefault()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}