#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace core
{
    // 网格顶点数据结构
    struct MeshVertex
    {
        glm::vec3 __position__{0.f, 0.f, 0.f};
        glm::vec2 __texCoord__{0.f, 0.f}; // uv坐标
        glm::vec3 __normal__{0.f, 1.f, 0.f};
    };

    class Mesh final
    {
    private:
        GLuint mVAO{0};
        GLuint mVBO{0};
        GLuint mEBO{0};

        uint32_t mVertexCount{0}; // 顶点数量
        uint32_t mIndexCount{0};  // 索引数量

    private:
        /// @brief 释放资源
        void Release();

    public:
        Mesh() = default;
        ~Mesh();

        // 禁止拷贝构造和拷贝赋值, 确保资源唯一管理, 放止误用导致的资源泄漏或双重释放
        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        /// @brief 移动构造函数
        /// @param other 右值引用, 用于移动数据
        Mesh(Mesh &&other) noexcept;

        /// @brief 移动赋值运算符
        /// @param other 右值引用, 用于移动数据
        /// @return 移动后的Mesh对象引用
        Mesh &operator=(Mesh &&other) noexcept;

        /// @brief 上传CPU数据到GPU并配置VAO属性布局
        /// @param vertices
        /// @param indices
        void SetData(const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices);

        /// @brief 绑定VAO, 准备绘制
        void Bind() const;

        /// @brief 解绑VAO, 恢复默认状态
        void Unbind() const;

        /// @brief 绘制网格
        void Draw() const;

        /// @brief 检查网格是否有效(VAO已创建且至少有一个顶点)
        /// @return true如果有效, 否则false
        bool IsValid() const { return mVAO != 0 && mVertexCount > 0; }

        /* getter */
        uint32_t GetVertexCount() const { return mVertexCount; }
        uint32_t GetIndexCount() const { return mIndexCount; }
        GLuint GetVAO() const { return mVAO; }

        // Primitive factories
        static std::unique_ptr<Mesh> CreatePlane(float size = 1.f);
        static std::unique_ptr<Mesh> CreateCube(float size = 1.f);
        static std::unique_ptr<Mesh> CreateSphere(float radius = 1.f, uint32_t latitude = 64, uint32_t longitude = 64);
    };
}