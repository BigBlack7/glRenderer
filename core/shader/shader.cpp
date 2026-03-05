#include "shader.hpp"
#include "utils/logger.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace core
{
    namespace detail
    {
        /// @brief 着色器文件根目录
        const std::filesystem::path ShaderRoot = "../../../assets/shader/";

        /// @brief 去除字符串左侧的空白字符
        /// @param s 输入字符串
        /// @return 去除左侧空白字符后的字符串
        static std::string TrimLeft(const std::string &s)
        {
            // 查找第一个非空白字符的位置(跳过空格、制表符、回车、换行)
            const auto pos = s.find_first_not_of(" \t\r\n");
            // 如果字符串全为空白字符, 返回空字符串, 否则返回从第一个非空白字符开始的子字符串
            return (pos == std::string::npos) ? "" : s.substr(pos);
        }
    }

    void Shader::CheckShaderError(GLuint target, CheckType type, std::string_view debugName)
    {
        GLint success = 0;
        GLint logLen = 0;
        if (type == CheckType::Compile)
        {
            // 检查着色器编译状态
            glGetShaderiv(target, GL_COMPILE_STATUS, &success);
            if (success)
                return;

            // 动态获取日志长度
            glGetShaderiv(target, GL_INFO_LOG_LENGTH, &logLen);
            std::string infoLog(static_cast<size_t>(logLen > 1 ? logLen : 1), '\0');
            glGetShaderInfoLog(target, logLen, nullptr, infoLog.data());
            GL_ERROR("[Shader] File Compile Error: {}\n{}", debugName, infoLog);
        }
        else
        {
            // 检查程序链接状态
            glGetProgramiv(target, GL_LINK_STATUS, &success);
            if (success)
                return;

            glGetProgramiv(target, GL_INFO_LOG_LENGTH, &logLen);
            std::string infoLog(static_cast<size_t>(logLen > 1 ? logLen : 1), '\0');
            glGetProgramInfoLog(target, logLen, nullptr, infoLog.data());

            GL_ERROR("[Shader] Program Link Error: {}\n{}", debugName, infoLog);
        }
    }

    std::filesystem::path Shader::ResolveShaderPath(std::string_view filePath)
    {
        std::filesystem::path p(filePath);
        if (p.is_absolute()) // 如果是绝对路径则直接返回
            return p;
        return detail::ShaderRoot / p;
    }

    std::string Shader::LoadShaderRecursive(const std::filesystem::path &filePath, std::unordered_set<std::string> &includeGuard)
    {
        // 规范化路径, 避免同一路径不同写法绕过检测
        const auto normalized = std::filesystem::weakly_canonical(filePath).string();

        // 循环包含检测: A -> B -> A, 检查当前文件是否已在处理栈中
        if (includeGuard.find(normalized) != includeGuard.end())
        {
            GL_CRITICAL("[Shader] #include Cycle Detected: {}", normalized);
        }

        // 打开文件
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            GL_CRITICAL("[Shader] File Open Failed: {}", filePath.string());
        }

        // 将当前文件加入"正在处理"集合, 防止递归调用时的循环包含
        includeGuard.insert(normalized);

        std::stringstream shaderStream;
        std::string line;
        int lineNo = 0;

        while (std::getline(file, line))
        {
            ++lineNo;
            const std::string trimmed = detail::TrimLeft(line);

            // 处理#include指令
            if (trimmed.rfind("#include", 0) == 0)
            {
                // 解析 #include "xxx.glsl"
                const auto start = trimmed.find('"');
                const auto end = trimmed.find_last_of('"');
                if (start == std::string::npos || end == std::string::npos || end <= start + 1)
                {
                    // 解析失败, 抛出异常并从includeGuard中移除当前文件, 以允许后续正确包含
                    includeGuard.erase(normalized);
                    GL_CRITICAL("[Shader] Bad #include Syntax: {}:{}\n{}", filePath.string(), lineNo, line);
                }

                // 按当前文件目录解析相对路径(获取文件名并拼接到当前文件目录)
                const auto includeRel = trimmed.substr(start + 1, end - start - 1);
                const auto includePath = filePath.parent_path() / includeRel;
                // 递归拼接被包含文件内容
                shaderStream << LoadShaderRecursive(includePath, includeGuard);
            }
            else
            {
                // 普通行直接拼接
                shaderStream << line << '\n';
            }
        }

        includeGuard.erase(normalized);
        return shaderStream.str();
    }

    std::string Shader::LoadShader(std::string_view filePath)
    {
        // 为每个顶层文件加载创建独立的循环包含检测集合
        std::unordered_set<std::string> includeGuard;
        const auto fullPath = ResolveShaderPath(filePath);
        GL_TRACE("[Shader] Load File: {}", fullPath.string());
        return LoadShaderRecursive(fullPath, includeGuard);
    }

    GLint Shader::GetUniformLocation(std::string_view name) const
    {
        // 缓存glGetUniformLocation()的结果, 避免重复查询OpenGL驱动, 从昂贵的系统调用变为O(1)哈希表查找
        const std::string key(name);

        if (const auto it = mUniformLocationCache.find(key); it != mUniformLocationCache.end())
            return it->second;

        // 缓存未命中, 必须查询OpenGL驱动
        const GLint location = glGetUniformLocation(mProgram, key.c_str());
        // 缓存查询结果, 无论是否找到, 都缓存起来, 避免重复查询
        mUniformLocationCache.emplace(key, location);

        if (location < 0)
        {
            // 记录未找到的uniform变量, 避免重复警告刷屏
            if (mMissingUniformWarned.insert(key).second)
            {
                GL_WARN("[Shader] Uniform Not Found or Optimized Out: {}", key);
            }
        }

        // 未找到时返回-1
        return location;
    }

    void Shader::Release() noexcept
    {
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
    }

    Shader::Shader(std::string_view vertexPath, std::string_view fragmentPath)
    {
        // 装载着色器源代码的字符串
        std::string vertexCode = LoadShader(vertexPath);
        std::string fragmentCode = LoadShader(fragmentPath);

        const char *vertexShaderSource = vertexCode.c_str();
        const char *fragmentShaderSource = fragmentCode.c_str();

        GLuint vertex = 0;
        GLuint fragment = 0;

        // 生成顶点与片元着色器句柄
        vertex = glCreateShader(GL_VERTEX_SHADER);
        fragment = glCreateShader(GL_FRAGMENT_SHADER);

        // 绑定着色器源代码到着色器句柄
        glShaderSource(vertex, 1, &vertexShaderSource, nullptr);
        glShaderSource(fragment, 1, &fragmentShaderSource, nullptr);

        // 编译着色器代码以及检查错误
        glCompileShader(vertex);
        CheckShaderError(vertex, CheckType::Compile, vertexPath);
        glCompileShader(fragment);
        CheckShaderError(fragment, CheckType::Compile, fragmentPath);

        // 创建着色器程序句柄
        mProgram = glCreateProgram();
        // 将编译后的着色器代码绑定到着色器程序句柄
        glAttachShader(mProgram, vertex);
        glAttachShader(mProgram, fragment);
        // 链接着色器程序以及检查错误
        glLinkProgram(mProgram);
        CheckShaderError(mProgram, CheckType::Link, "Program");

        // 删除着色器对象, 因为它们已经链接到程序对象上了, 不再需要
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    Shader::~Shader() noexcept
    {
        Release();
    }

    void Shader::Begin() const noexcept
    {
        glUseProgram(mProgram);
    }

    void Shader::End() const noexcept
    {
        glUseProgram(0);
    }

    /* setter */
    void Shader::SetFloat(std::string_view name, float value) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniform1f(location, value);
    }

    void Shader::SetVec3(std::string_view name, float x, float y, float z) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniform3f(location, x, y, z);
    }

    void Shader::SetVec3(std::string_view name, const float *value) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniform3fv(location, 1, value); // 第二个参数count表示要设置的vec3数量, 这里是1个vec3, 因此传入1
    }

    void Shader::SetVec3(std::string_view name, const glm::vec3 &value) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniform3fv(location, 1, glm::value_ptr(value)); // glm::vec3在内存中是连续存储的, 可以直接传入地址
    }

    void Shader::SetInt(std::string_view name, int value) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniform1i(location, value);
    }

    void Shader::SetUInt(std::string_view name, uint32_t value) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniform1ui(location, value);
    }

    void Shader::SetMat4(std::string_view name, const glm::mat4 &value) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value)); // glm::mat4在内存中是连续存储的, 可以直接传入地址; GL_FALSE表示不进行转置, 因为glm默认是列主序(OpenGL也是列主序)
    }

    void Shader::SetMat3(std::string_view name, const glm::mat3 &value) const
    {
        const GLint location = GetUniformLocation(name);
        if (location >= 0)
            glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }
}