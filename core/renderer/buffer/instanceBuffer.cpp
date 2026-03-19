#include "instanceBuffer.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <utility>

namespace core
{
    void InstanceBuffer::Release() noexcept
    {
        if (mVBO != 0)
        {
            glDeleteBuffers(1, &mVBO);
            mVBO = 0;
        }
        mCapacityBytes = 0;
    }

    InstanceBuffer::~InstanceBuffer()
    {
        Release();
    }

    InstanceBuffer::InstanceBuffer(InstanceBuffer &&other) noexcept
    {
        *this = std::move(other);
    }

    InstanceBuffer &InstanceBuffer::operator=(InstanceBuffer &&other) noexcept
    {
        if (this == &other)
            return *this;

        Release();

        mVBO = std::exchange(other.mVBO, 0u);
        mCapacityBytes = std::exchange(other.mCapacityBytes, static_cast<size_t>(0));
        return *this;
    }

    bool InstanceBuffer::Reserve(size_t bytes)
    {
        if (bytes == 0)
            return true;

        if (mVBO == 0)
            glGenBuffers(1, &mVBO);

        if (mVBO == 0)
        {
            GL_ERROR("[InstanceBuffer] Reserve Failed: VBO Creation Failed");
            return false;
        }

        if (bytes <= mCapacityBytes)
            return true;

        const size_t newCapacity = std::max(bytes, (mCapacityBytes > 0 ? mCapacityBytes * 2 : bytes));

        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(newCapacity), nullptr, GL_DYNAMIC_DRAW); // 分配内存
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        mCapacityBytes = newCapacity;
        return true;
    }

    bool InstanceBuffer::Upload(const void *data, size_t bytes)
    {
        if (bytes == 0)
            return true;

        if (!data)
        {
            GL_ERROR("[InstanceBuffer] Upload Failed: Data Is Null");
            return false;
        }

        if (!Reserve(bytes))
            return false;

        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), data); // 上传数据
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }

    void InstanceBuffer::Bind() const noexcept
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    }

    void InstanceBuffer::Unbind() noexcept
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}