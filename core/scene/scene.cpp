#include "scene.hpp"

namespace core
{
    namespace detail
    {
        constexpr float Epsilon = 1e-6f;
    }

    bool Scene::IsValidID(EntityID id) const noexcept
    {
        // 检查ID是否在有效范围内且对应的optional包含实体
        return id < static_cast<EntityID>(mEntities.size()) && mEntities[id].has_value();
    }

    bool Scene::WouldCreateCycle(EntityID child, EntityID newParent) const noexcept
    {
        // 无效ID检查
        if (child == InvalidEntityID || newParent == InvalidEntityID)
            return false;

        // 向上遍历新父实体的祖先链, 检查是否成环
        EntityID p = newParent;
        while (p != InvalidEntityID)
        {
            // 如果在祖先链中找到child, 则会产生循环引用
            if (p == child)
                return true;

            // 遇到无效节点时停止遍历
            if (!IsValidID(p))
                break;

            p = mNodes[p].__parent__;
        }
        return false;
    }

    void Scene::RebuildRootCache()
    {
        // 如果缓存未失效, 直接返回
        if (!mRootsDirty)
            return;

        // 清空并预留空间以提高性能
        mRootCache.clear();
        mRootCache.reserve(mEntities.size());

        // 遍历所有实体, 找出根实体
        for (EntityID id = 0; id < static_cast<EntityID>(mEntities.size()); ++id)
        {
            if (!mEntities[id].has_value()) // 跳过空槽位
                continue;

            if (mNodes[id].__parent__ == InvalidEntityID) // 根实体条件
                mRootCache.push_back(id);
        }
        mRootsDirty = false; // 标记缓存为最新
    }

    void Scene::UpdateNodeRecursive(EntityID id, const glm::mat4 &parentWorld)
    {
        // 获取实体指针, 无效则返回
        Entity *entity = GetEntity(id);
        if (!entity)
            return;

        // 计算世界变换矩阵: 父世界矩阵 × 本地变换矩阵
        const glm::mat4 &local = entity->GetTransform().GetLocalMatrix();
        const glm::mat4 world = parentWorld * local;
        mNodes[id].__worldMatrix__ = world;

        // 计算法线矩阵
        const glm::mat3 m3(world);
        const float det = glm::determinant(m3);
        if (glm::abs(det) <= detail::Epsilon) // 如果行列式接近零, 使用单位矩阵避免数值问题
            mNodes[id].__worldNormalMatrix__ = glm::mat3(1.f);
        else
            mNodes[id].__worldNormalMatrix__ = glm::transpose(glm::inverse(m3)); // 法线矩阵 = (模型矩阵的逆转置)的3x3部分

        // 递归更新所有子节点
        for (EntityID child : mNodes[id].__children__)
        {
            if (IsValidID(child))
                UpdateNodeRecursive(child, world);
        }
    }

    EntityID Scene::CreateEntity(std::string name)
    {
        EntityID id = InvalidEntityID;

        // 优先重用空闲ID以提高性能
        if (!mFreeIDs.empty())
        {
            id = mFreeIDs.back(); // 获取最后一个空闲ID
            mFreeIDs.pop_back();  // 从空闲列表移除

            // 在现有位置构造新实体并初始化节点信息
            mEntities[id].emplace(id, std::move(name));
            mNodes[id] = Node{};
        }
        else
        {
            // 没有空闲ID时, 扩展容器
            id = static_cast<EntityID>(mEntities.size());
            mEntities.emplace_back(std::in_place, id, std::move(name));
            mNodes.emplace_back(); // 添加对应节点
        }

        mRootsDirty = true; // 根缓存需要重建
        return id;
    }

    bool Scene::DestroyEntity(EntityID id)
    {
        if (!IsValidID(id)) // 无效ID检查
            return false;

        // 递归销毁所有子实体
        const auto childrenCopy = mNodes[id].__children__;
        for (EntityID child : childrenCopy)
            DestroyEntity(child);

        // 从父节点分离当前实体并清空子节点列表
        Detach(id);
        mNodes[id].__children__.clear();

        // 重置实体槽位并回收ID
        mEntities[id].reset();
        mFreeIDs.push_back(id);

        mRootsDirty = true; // 根缓存需要重建
        return true;
    }

    Entity *Scene::GetEntity(EntityID id) noexcept
    {
        if (!IsValidID(id))
            return nullptr;
        return &mEntities[id].value();
    }

    const Entity *Scene::GetEntity(EntityID id) const noexcept
    {
        if (!IsValidID(id))
            return nullptr;
        return &mEntities[id].value();
    }

    bool Scene::Reparent(EntityID child, EntityID newParent)
    {
        if (!IsValidID(child))
            return false;

        if (newParent != InvalidEntityID && !IsValidID(newParent))
            return false;

        if (newParent == child) // 不能将自己作为父实体
            return false;

        if (WouldCreateCycle(child, newParent)) // 循环引用检查
            return false;

        const EntityID oldParent = mNodes[child].__parent__;
        if (oldParent == newParent) // 已经是目标父实体
            return true;

        // 从旧父实体的子列表中移除当前实体
        if (oldParent != InvalidEntityID && IsValidID(oldParent))
        {
            auto &siblings = mNodes[oldParent].__children__;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
        }

        // 设置新的父实体
        mNodes[child].__parent__ = newParent;

        // 添加到新父实体的子列表
        if (newParent != InvalidEntityID)
        {
            auto &children = mNodes[newParent].__children__;
            if (std::find(children.begin(), children.end(), child) == children.end())
                children.push_back(child);
        }

        mRootsDirty = true; // 根缓存需要重建
        return true;
    }

    bool Scene::Detach(EntityID child)
    {
        // 分离操作即为重置父关系到无效ID
        return Reparent(child, InvalidEntityID);
    }

    EntityID Scene::GetParent(EntityID id) const noexcept
    {
        if (!IsValidID(id))
            return InvalidEntityID;

        return mNodes[id].__parent__;
    }

    std::span<const EntityID> Scene::GetChildren(EntityID id) const noexcept
    {
        if (!IsValidID(id))
            return {};

        return mNodes[id].__children__;
    }

    void Scene::UpdateWorldMatrices()
    {
        RebuildRootCache(); // 确保根缓存是最新的
        for (EntityID root : mRootCache)
            UpdateNodeRecursive(root, glm::mat4(1.f));
    }

    const glm::mat4 &Scene::GetWorldMatrix(EntityID id) const noexcept
    {
        static const glm::mat4 Identity(1.f);
        if (!IsValidID(id))
            return Identity;

        return mNodes[id].__worldMatrix__;
    }

    const glm::mat3 &Scene::GetWorldNormalMatrix(EntityID id) const noexcept
    {
        static const glm::mat3 Identity(1.f);
        if (!IsValidID(id))
            return Identity;
        return mNodes[id].__worldNormalMatrix__;
    }

    void Scene::AddDirectionalLight(const DirectionalLight &light)
    {
        mDirectionalLights.push_back(light);
    }

    void Scene::ClearDirectionalLights()
    {
        mDirectionalLights.clear();
    }

    void Scene::AddPointLight(const PointLight &light)
    {
        mPointLights.push_back(light);
    }

    void Scene::ClearPointLights()
    {
        mPointLights.clear();
    }

    void Scene::AddSpotLight(const SpotLight &light)
    {
        mSpotLights.push_back(light);
    }

    void Scene::ClearSpotLights()
    {
        mSpotLights.clear();
    }
}