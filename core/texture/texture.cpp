#include "texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>

namespace core
{

    namespace detail
    {
        const std::string TextureRoot = "../../../assets/texture/";
    }

    Texture::Texture(const std::string &path, uint32_t unit)
    {
        mUnit = unit;
        uint32_t channels;
        stbi_set_flip_vertically_on_load(true); // 翻转y轴, 因为OpenGL的纹理坐标系原点在左下角, 而图片的原点通常在左上角
        unsigned char *data = stbi_load((detail::TextureRoot + path).c_str(), (int *)&mWidth, (int *)&mHeight, (int *)&channels, STBI_rgb_alpha);

        // 创建纹理对象并绑定纹理
        glGenTextures(1, &mTextureID);
        glActiveTexture(GL_TEXTURE0 + mUnit); // 激活纹理单元, 后续的纹理操作都将对该纹理单元进行操作
        glBindTexture(GL_TEXTURE_2D, mTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data); // 传输图片数据到纹理对象中, 开辟显存
        glGenerateMipmap(GL_TEXTURE_2D);                                                              // 生成纹理的mipmap, 提升远处纹理采样质量

        // 释放纹理资源
        stbi_image_free(data);

        // 设置纹理过滤方式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);                // 需要像素 > 纹理像素, 双线性插值
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR); // 需要像素 < 纹理像素, 最近邻插值(mipmap层级之间为线性插值)

        // 设置纹理环绕方式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S轴(水平方向u)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T轴(垂直方向v)
    }

    Texture::~Texture()
    {
        glDeleteTextures(1, &mTextureID);
    }
}