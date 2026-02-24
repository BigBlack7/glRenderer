#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

namespace core
{
    class Texture
    {
    private:
        GLuint mTextureID{0};
        uint32_t mWidth{0};
        uint32_t mHeight{0};
        uint32_t mUnit{0};

    public:
        Texture(const std::string &path, uint32_t unit);
        ~Texture();

        void Bind();

        /* getter */
        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }
    };
}