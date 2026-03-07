#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>

namespace core
{
    // shader文件的编码格式必须为UTF-8, 其它类型可能导致文件开头不是预期的字符, 从而导致编译错误
    class Shader
    {
    private:
        GLuint mProgram{0};

        mutable std::unordered_map<std::string, GLint> mUniformLocationCache{};
        mutable std::unordered_set<std::string> mMissingUniformWarned{};
        mutable std::unordered_set<std::string> mMissingUniformBlockWarned{};

        enum class CheckType
        {
            Compile,
            Link
        };

    private:
        /// @brief 检查着色器编译或链接错误
        /// @param target 着色器对象
        /// @param type 检查类型
        /// @param debugName 调试名称
        void CheckShaderError(GLuint target, CheckType type, std::string_view debugName);

        /// @brief 解析着色器文件路径, 如果是绝对路径则直接返回, 否则将相对路径与ShaderRoot拼接后返回
        /// @param filePath 着色器文件路径
        /// @return 解析后的着色器文件路径
        static std::filesystem::path ResolveShaderPath(std::string_view filePath);

        /// @brief 递归加载着色器文件, 处理#include指令, 并使用includeGuard防止重复包含
        /// @param filePath 着色器文件路径
        /// @param includeGuard 包含守卫集合
        /// @return 加载后的着色器代码字符串
        std::string LoadShaderRecursive(const std::filesystem::path &filePath, std::unordered_set<std::string> &includeGuard);

        /// @brief 加载着色器文件, 处理#include指令, 并使用includeGuard防止重复包含
        /// @param filePath 着色器文件路径
        /// @return 加载后的着色器代码字符串
        std::string LoadShader(std::string_view filePath);

        /// @brief  获取uniform变量的位置, 使用缓存优化性能
        /// @param name uniform变量名
        /// @return uniform变量的位置, 如果不存在或被优化掉了则返回-1
        GLint GetUniformLocation(std::string_view name) const;

        /// @brief 释放着色器程序资源
        void Release() noexcept;

    public:
        Shader(std::string_view vertexPath, std::string_view fragmentPath);
        Shader(const char *vertexPath, const char *fragmentPath)
            : Shader(std::string_view(vertexPath), std::string_view(fragmentPath)) {}
        ~Shader() noexcept;

        // 禁止拷贝避免同一个GL Program被多个对象析构
        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;

        /* getter */
        GLuint GetID() const noexcept { return mProgram; }

        /// @brief 将着色器程序中的uniform块绑定到指定的绑定点上, 以便后续通过绑定点访问该uniform块的数据
        /// @param blockName uniform块名称
        /// @param bindingPoint 绑定点
        /// @return 是否绑定成功
        bool BindUniformBlock(std::string_view blockName, uint32_t bindingPoint) const;

        /* setter -> 通过着色器程序ID和uniform变量名获取uniform变量的位置, 然后设置uniform变量的值为value */
        void SetFloat(std::string_view name, float value) const;
        void SetVec3(std::string_view name, float x, float y, float z) const;
        void SetVec3(std::string_view name, const float *value) const;
        void SetVec3(std::string_view name, const glm::vec3 &value) const;
        void SetInt(std::string_view name, int value) const;
        void SetUInt(std::string_view name, uint32_t value) const;
        void SetMat4(std::string_view name, const glm::mat4 &value) const;
        void SetMat3(std::string_view name, const glm::mat3 &value) const;
    };
}
