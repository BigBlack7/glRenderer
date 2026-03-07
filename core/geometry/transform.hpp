#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace core
{
    class Transform final
    {
    private:
        glm::vec3 mPosition{0.f, 0.f, 0.f};      // 平移
        glm::quat mRotation{1.f, 0.f, 0.f, 0.f}; // 旋转
        glm::vec3 mScale{1.f, 1.f, 1.f};         // 缩放

        /*
            本地变换矩阵(model matrix) = 平移 * 旋转 * 缩放
            mutable允许在const成员函数中修改, 用于实现局部矩阵和法线矩阵的惰性计算和缓存, 逻辑上对象外部状态没变, 只是内部缓存刷新
            没有父节点时, 本地变换矩阵就是全局变换矩阵; 有父节点时为相对
        */
        mutable glm::mat4 mLocalMatrix{1.f}; // 等同于Model Matrix

        mutable bool mLocalDirty{true};
        bool mIsIdentity{true}; // 记录当前是否单位变换, 给上层做快速路径(比如可跳过乘法)

    private:
        /// @brief 任一TRS改变后, 标记缓存失效
        void MarkDirty();

        /// @brief 根据当前TRS重算是否为单位变换
        void UpdateIdentityFlag();

        /// @brief 根据当前TRS重算本地变换矩阵
        void RebuildLocalMatrix() const;

    public:
        Transform() = default;
        ~Transform() = default;

        /// @brief 重置TRS为单位变换, 方便复位和复用对象
        void Reset();

        // Position
        void SetPosition(const glm::vec3 &position);
        void Translate(const glm::vec3 &delta);
        glm::vec3 GetPosition() const { return mPosition; }

        // Rotation (Quaternion)
        void SetRotationQuat(const glm::quat &rotation);
        glm::quat GetRotationQuat() const { return mRotation; }

        // Rotation (Euler XYZ, 弧度)
        // 约定: eulerRad.x = 绕X轴(pitch), eulerRad.y = 绕Y轴(yaw), eulerRad.z = 绕Z轴(roll)
        // 顺序: XYZ
        void SetEulerXyzRad(const glm::vec3 &eulerRad);
        glm::vec3 GetEulerXyzRad() const;

        // Rotation (Euler XYZ, 角度)
        void SetEulerXyzDeg(const glm::vec3 &eulerDeg);
        glm::vec3 GetEulerXyzDeg() const;

        // Scale
        void SetScale(const glm::vec3 &scale);
        glm::vec3 GetScale() const { return mScale; }

        // Cached matrices
        const glm::mat4 &GetLocalMatrix() const;

        bool IsIdentity() const { return mIsIdentity; }
    };
}