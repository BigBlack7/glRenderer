#pragma once
#include <glad/glad.h>
#include <cstddef>
#include <cstdint>

namespace core
{
    class UniformBuffer final
    {
    private:
        GLuint mUBO{0};            // OpenGL UBO对象句柄
        GLsizeiptr mSize{0};       // 缓冲区总大小(字节)
        uint32_t mBindingPoint{0}; // 绑定槽位(0-15, 对应binding=0-15)

        void Release() noexcept;

    public:
        UniformBuffer() = default;

        /// @brief 创建一个Uniform Buffer对象
        /// @param size 缓冲区大小(字节)
        /// @param bindingPoint 绑定槽位(0-15, 对应binding=0-15)
        /// @param usage 缓冲区使用模式(GL_STATIC_DRAW, GL_DYNAMIC_DRAW等)
        UniformBuffer(GLsizeiptr size, uint32_t bindingPoint, GLenum usage = GL_DYNAMIC_DRAW);

        /// @brief 销毁Uniform Buffer对象
        ~UniformBuffer();

        // 禁止复制
        UniformBuffer(const UniformBuffer &) = delete;
        UniformBuffer &operator=(const UniformBuffer &) = delete;

        // 允许移动
        UniformBuffer(UniformBuffer &&other) noexcept;
        UniformBuffer &operator=(UniformBuffer &&other) noexcept;

        /// @brief 创建Uniform Buffer对象
        /// @param size 缓冲区大小(字节)
        /// @param bindingPoint 绑定槽位(0-15, 对应binding=0-15)
        /// @param usage 缓冲区使用模式(GL_STATIC_DRAW, GL_DYNAMIC_DRAW等)
        /// @return 是否创建成功
        bool Create(GLsizeiptr size, uint32_t bindingPoint, GLenum usage = GL_DYNAMIC_DRAW);

        /// @brief 更新Uniform Buffer的部分数据
        /// @param offset 偏移量(字节)
        /// @param size 更新数据大小(字节)
        /// @param data 数据指针
        void SetData(GLintptr offset, GLsizeiptr size, const void *data) const;

        /// @brief 更新整个Uniform Buffer的数据
        /// @param size 缓冲区大小(字节)
        /// @param data 数据指针
        void SetData(GLsizeiptr size, const void *data) const { SetData(0, size, data); }

        /// @brief 绑定Uniform Buffer对象到当前上下文
        void Bind() const;

        /// @brief 解绑当前绑定的Uniform Buffer对象
        void Unbind() const;

        bool IsValid() const noexcept { return mUBO != 0; }
        GLuint GetID() const noexcept { return mUBO; }
        GLsizeiptr GetSize() const noexcept { return mSize; }
        uint32_t GetBindingPoint() const noexcept { return mBindingPoint; }
    };
}