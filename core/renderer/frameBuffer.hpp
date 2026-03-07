#pragma once
#include <glad/glad.h>
#include <cstdint>

namespace core
{
    class FrameBuffer final
    {
    private:
        GLuint mFbo{0};
        GLuint mColor{0};
        GLuint mDepthRbo{0};

        uint32_t mWidth{0};
        uint32_t mHeight{0};
        bool mWithDepth{true};

        void Release() noexcept;

    public:
        FrameBuffer() = default;
        ~FrameBuffer();

        FrameBuffer(const FrameBuffer &) = delete;
        FrameBuffer &operator=(const FrameBuffer &) = delete;
        FrameBuffer(FrameBuffer &&other) noexcept;
        FrameBuffer &operator=(FrameBuffer &&other) noexcept;

        bool Create(uint32_t width, uint32_t height, bool withDepth = true);
        void Resize(uint32_t width, uint32_t height);

        void Bind() const;
        static void BindDefault();

        bool IsValid() const noexcept { return mFbo != 0; }
        GLuint GetColorAttachment() const noexcept { return mColor; }
        uint32_t GetWidth() const noexcept { return mWidth; }
        uint32_t GetHeight() const noexcept { return mHeight; }
    };
}