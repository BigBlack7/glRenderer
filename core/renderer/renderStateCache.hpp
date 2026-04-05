#pragma once
#include <glad/glad.h>
#include <array>
#include <cstdint>

namespace core
{
    // 缓存OpenGL状态避免冗余的状态切换调用, 提升渲染性能
    class RenderStateCache final
    {
    private:
        static constexpr uint32_t MaxTextureUnits = 32u; // 支持最大纹理单元数

        GLuint mProgram{0};                                    // 当前绑定的着色器程序(0表示无绑定)
        GLuint mVao{0};                                        // 当前绑定的VAO(0表示无绑定)
        std::array<GLuint, MaxTextureUnits> mTexture2D{};      // 各纹理单元的2D纹理绑定状态(0表示无绑定)
        std::array<GLuint, MaxTextureUnits> mTexture2DArray{}; // 各纹理单元的2D数组纹理绑定状态(0表示无绑定)
        std::array<GLuint, MaxTextureUnits> mTextureCube{};    // 各纹理单元的立方体纹理绑定状态(0表示无绑定)

    public:
        /// @brief 默认构造, 初始化所有状态为默认值(0表示未绑定任何对象)
        RenderStateCache();

        /// @brief 重置所有状态为默认值
        void Reset();

        /// @brief 使用着色器程序(带缓存检查)
        /// @param program 着色器程序ID
        /// @return 如果实际执行了glUseProgram返回true, 如果命中缓存返回false
        bool UseProgram(GLuint program);

        /// @brief 绑定VAO(带缓存检查)
        /// @param vao VAO ID
        /// @return 如果实际执行了glBindVertexArray返回true, 如果命中缓存返回false
        bool BindVertexArray(GLuint vao);

        /// @brief 绑定2D纹理(带缓存检查)
        /// @param unit 纹理单元索引(0~31)
        /// @param textureID 2D纹理ID
        /// @return 如果实际执行了glBindTexture返回true, 如果命中缓存返回false
        bool BindTexture2D(uint32_t unit, GLuint textureID);

        /// @brief 绑定2D数组纹理(带缓存检查)
        /// @param unit 纹理单元索引(0~31)
        /// @param textureID 2D数组纹理ID
        /// @return 如果实际执行了glBindTexture返回true, 如果命中缓存返回false
        bool BindTexture2DArray(uint32_t unit, GLuint textureID);

        /// @brief 绑定立方体纹理(带缓存检查)
        /// @param unit 纹理单元索引(0~31)
        /// @param textureID 立方体纹理ID
        /// @return 如果实际执行了glBindTexture返回true, 如果命中缓存返回false
        bool BindTextureCube(uint32_t unit, GLuint textureID);
    };
}