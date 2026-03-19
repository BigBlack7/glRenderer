#pragma once
#include <glad/glad.h>
#include <cstddef>

namespace core
{
    class InstanceBuffer final
    {
    private:
        GLuint mVBO{0};
        size_t mCapacityBytes{0};

        /// @brief 
        void Release() noexcept;

    public:
        InstanceBuffer() = default;
        ~InstanceBuffer();

        InstanceBuffer(const InstanceBuffer &) = delete;
        InstanceBuffer &operator=(const InstanceBuffer &) = delete;

        InstanceBuffer(InstanceBuffer &&other) noexcept;
        InstanceBuffer &operator=(InstanceBuffer &&other) noexcept;

        /// @brief 预留内存
        /// @param bytes 需要的字节数
        /// @return 是否预留成功
        bool Reserve(size_t bytes);

        /// @brief 上传数据到缓冲区
        /// @param data 数据指针
        /// @param bytes 数据字节数
        /// @return 是否上传成功
        bool Upload(const void *data, size_t bytes);

        /// @brief 
        void Bind() const noexcept;

        /// @brief 
        static void Unbind() noexcept;

        bool IsValid() const noexcept { return mVBO != 0; }
        GLuint GetID() const noexcept { return mVBO; }
        size_t GetCapacityBytes() const noexcept { return mCapacityBytes; }
    };
}