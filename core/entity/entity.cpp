#include "entity.hpp"
#include "shader/shader.hpp"
#include <algorithm>
#include <cmath>

namespace core
{
    namespace detail
    {
        constexpr float Epsilon = 1e-6f;

        static bool IsIdentityMatrix(const glm::mat4 &m)
        {
            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    const float expected = (c == r) ? 1.f : 0.f;
                    if (std::abs(m[c][r] - expected) > Epsilon)
                        return false;
                }
            }
            return true;
        }
    }

    Entity::Entity(EntityID id, std::string name) : mID(id), mName(std::move(name))
    {
    }

    void Entity::SetParent(EntityID parent) noexcept
    {
        if (parent == mID) // 不能设置为自己作为父节点
            return;
        mParent = parent; // 直接设置父ID, 关系一致性由上层Scene维护
    }

    bool Entity::AddChild(EntityID child)
    {
        if (child == InvalidEntityID || child == mID) // 非法ID或自引用
            return false;

        // 查重
        const auto it = std::find(mChildren.begin(), mChildren.end(), child);
        if (it != mChildren.end())
            return false;

        mChildren.push_back(child);
        return true;
    }

    bool Entity::RemoveChild(EntityID child)
    {
        // 查找目标
        const auto it = std::find(mChildren.begin(), mChildren.end(), child);
        if (it == mChildren.end()) // 不存在
            return false;

        mChildren.erase(it);
        return true;
    }

    void Entity::SetActive(bool active) noexcept
    {
        if (active)
            mFlags |= FlagActive; // 激活
        else
            mFlags &= static_cast<uint8_t>(~FlagActive); // 停用
    }

    bool Entity::IsActive() const noexcept
    {
        return (mFlags & FlagActive) != 0u;
    }

    void Entity::SetVisible(bool visible) noexcept
    {
        if (visible)
            mFlags |= FlagVisible; // 可见
        else
            mFlags &= static_cast<uint8_t>(~FlagVisible); // 不可见
    }

    bool Entity::IsVisible() const noexcept
    {
        return (mFlags & FlagVisible) != 0u;
    }

    bool Entity::CanRender() const noexcept
    {
        return IsActive() && IsVisible() && mMesh && mMesh->IsValid() && mMaterial && mMaterial->GetShader();
    }

    glm::mat4 Entity::BuildWorldMatrix(const glm::mat4 &parentWorld) const
    {
        if (mTransform.IsIdentity())
            return parentWorld;

        const glm::mat4 &local = mTransform.GetModelMatrix();
        if (detail::IsIdentityMatrix(parentWorld))
            return local;

        return parentWorld * local;
    }

    glm::mat3 Entity::BuildNormalMatrix(const glm::mat4 &parentWorld) const
    {
        // 复用Transform内部缓存(局部法线矩阵)
        const glm::mat3 &localNormal = mTransform.GetNormalMatrix();

        // 无父变换时直接返回局部法线矩阵
        if (detail::IsIdentityMatrix(parentWorld))
            return localNormal;

        // 层级组合: N(world) = N(parent) * N(local)
        const glm::mat3 parentMat3(parentWorld);
        const float det = glm::determinant(parentMat3);
        if (std::abs(det) <= detail::Epsilon)
            return localNormal;

        const glm::mat3 parentNormal = glm::transpose(glm::inverse(parentMat3));
        return parentNormal * localNormal;
    }

    void Entity::Draw(const Camera &camera, const glm::mat4 &parentWorld) const
    {
        if (!CanRender())
            return;

        const glm::mat4 world = BuildWorldMatrix(parentWorld);
        const glm::mat3 normal = BuildNormalMatrix(parentWorld);

        mMaterial->Bind();
        const auto &shader = mMaterial->GetShader();

        shader->SetMat4("uP", camera.GetProjectionMatrix());
        shader->SetMat4("uV", camera.GetViewMatrix());
        shader->SetMat4("uM", world);
        shader->SetMat3("uN", normal);
        shader->SetVec3("uViewPos", camera.GetPosition());

        mMesh->Draw();
        mMaterial->Unbind();
    }
}