#include "transform.hpp"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/common.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace core
{
    namespace detail
    {
        /// @brief 浮点比较阈值
        constexpr float Epsilon = 1e-6f;
    }

    void Transform::MarkDirty()
    {
        mLocalDirty = true;
    }

    void Transform::UpdateIdentityFlag()
    {
        // TODO 实现更高效的方案
        const bool posIdentity = glm::length2(mPosition) <= (detail::Epsilon * detail::Epsilon);                 // 位置是否近似零向量
        const bool scaleIdentity = glm::length2(mScale - glm::vec3(1.f)) <= (detail::Epsilon * detail::Epsilon); // 缩放是否近似(1, 1, 1)

        const glm::quat q = glm::normalize(mRotation); // 旋转先单位化再判断, 避免误差传播
        const bool rotIdentity =
            glm::abs(q.x) <= detail::Epsilon &&
            glm::abs(q.y) <= detail::Epsilon &&
            glm::abs(q.z) <= detail::Epsilon &&
            glm::abs(glm::abs(q.w) - 1.f) <= detail::Epsilon; // q 与 -q 表示同一旋转

        mIsIdentity = posIdentity && scaleIdentity && rotIdentity;
    }

    void Transform::RebuildLocalMatrix() const
    {
        if (!mLocalDirty)
            return;

        const glm::mat4 t = glm::translate(glm::mat4(1.f), mPosition);
        const glm::mat4 r = glm::mat4_cast(mRotation);
        const glm::mat4 s = glm::scale(glm::mat4(1.f), mScale);

        mLocalMatrix = t * r * s;
        mLocalDirty = false;
    }

    void Transform::Reset()
    {
        mPosition = glm::vec3(0.f, 0.f, 0.f);
        mRotation = glm::quat(1.f, 0.f, 0.f, 0.f);
        mScale = glm::vec3(1.f, 1.f, 1.f);

        MarkDirty();
        UpdateIdentityFlag();
    }

    void Transform::SetPosition(const glm::vec3 &position)
    {
        mPosition = position;
        MarkDirty();
        UpdateIdentityFlag();
    }

    void Transform::Translate(const glm::vec3 &delta)
    {
        mPosition += delta;
        MarkDirty();
        UpdateIdentityFlag();
    }

    void Transform::SetRotationQuat(const glm::quat &rotation)
    {
        mRotation = glm::normalize(rotation);
        MarkDirty();
        UpdateIdentityFlag();
    }

    void Transform::SetEulerXyzRad(const glm::vec3 &eulerRad)
    {
        // eulerRad.x/y/z 分别是绕 X/Y/Z 轴旋转，顺序 XYZ
        const glm::mat4 rot = glm::eulerAngleXYZ(eulerRad.x, eulerRad.y, eulerRad.z);
        mRotation = glm::normalize(glm::quat_cast(rot));

        MarkDirty();
        UpdateIdentityFlag();
    }

    glm::vec3 Transform::GetEulerXyzRad() const
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        const glm::mat4 rot = glm::mat4_cast(glm::normalize(mRotation));
        glm::extractEulerAngleXYZ(rot, x, y, z);
        return glm::vec3(x, y, z);
    }

    void Transform::SetEulerXyzDeg(const glm::vec3 &eulerDeg)
    {
        SetEulerXyzRad(glm::radians(eulerDeg));
    }

    glm::vec3 Transform::GetEulerXyzDeg() const
    {
        return glm::degrees(GetEulerXyzRad());
    }

    void Transform::SetScale(const glm::vec3 &scale)
    {
        mScale = scale;
        MarkDirty();
        UpdateIdentityFlag();
    }

    const glm::mat4 &Transform::GetLocalMatrix() const
    {
        RebuildLocalMatrix();
        return mLocalMatrix;
    }
}