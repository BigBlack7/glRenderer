#include "entity.hpp"
#include "shader/shader.hpp"

namespace core
{
    namespace detail
    {
        constexpr float Epsilon = 1e-6f;
    }

    Entity::Entity(EntityID id, std::string name) : mID(id), mName(std::move(name))
    {
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
}