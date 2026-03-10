#include "uniformBuffer.hpp"
#include "utils/logger.hpp"
#include <utility>

namespace core
{
    void UniformBuffer::Release() noexcept
    {
        if (mUBO != 0)
        {
            glDeleteBuffers(1, &mUBO);
            mUBO = 0;
        }
        mSize = 0;
        mBindingPoint = 0;
    }

    UniformBuffer::UniformBuffer(GLsizeiptr size, uint32_t bindingPoint, GLenum usage)
    {
        Create(size, bindingPoint, usage);
    }

    UniformBuffer::~UniformBuffer()
    {
        Release();
    }

    UniformBuffer::UniformBuffer(UniformBuffer &&other) noexcept
    {
        *this = std::move(other);
    }

    UniformBuffer &UniformBuffer::operator=(UniformBuffer &&other) noexcept
    {
        if (this == &other)
            return *this;

        Release();

        mUBO = other.mUBO;
        mSize = other.mSize;
        mBindingPoint = other.mBindingPoint;

        other.mUBO = 0;
        other.mSize = 0;
        other.mBindingPoint = 0;
        return *this;
    }

    bool UniformBuffer::Create(GLsizeiptr size, uint32_t bindingPoint, GLenum usage)
    {
        if (size <= 0)
        {
            GL_ERROR("[UniformBuffer] Create Failed: Size <= 0");
            return false;
        }

        Release();

        glGenBuffers(1, &mUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
        glBufferData(GL_UNIFORM_BUFFER, size, nullptr, usage);   // 预分配缓冲区内存, nullptr只分配空间不初始化数据
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, mUBO); // 绑定到指定槽位
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        mSize = size;
        mBindingPoint = bindingPoint;
        return true;
    }

    void UniformBuffer::SetData(GLintptr offset, GLsizeiptr size, const void *data) const
    {
        if (!IsValid() || data == nullptr || offset < 0 || size <= 0 || (offset + size) > mSize)
        {
            GL_ERROR("[UniformBuffer] SetData Failed: Invalid Buffer Range");
            return;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data); // 更新子区域(只更新变化的部分, 减少GPU带宽)
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void UniformBuffer::Bind() const
    {
        if (IsValid())
            glBindBufferBase(GL_UNIFORM_BUFFER, mBindingPoint, mUBO);
    }

    void UniformBuffer::Unbind() const
    {
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
}