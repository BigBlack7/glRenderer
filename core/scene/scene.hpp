#pragma once
#include "entity.hpp"
#include "light/lightManager.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace core
{
    class Scene final
    {
    private:
        struct Node final
        {
            EntityID __parent__{InvalidEntityID};
            std::vector<EntityID> __children__{};
            glm::mat4 __worldMatrix__{1.f};
            glm::mat3 __worldNormalMatrix__{1.f};
        };

        std::vector<std::optional<Entity>> mEntities{}; // 实体存储池, 使用optional允许空槽位
        std::vector<Node> mNodes{};                     // 对应的节点信息池
        std::vector<EntityID> mFreeIDs{};               // 空闲ID列表, 用于实体销毁后的ID重用

        std::vector<EntityID> mRootCache{}; // 缓存所有根实体
        bool mRootsDirty{true};             // 根缓存是否需要重建

        class LightManager mLightManager{};

    private:
        /// @brief 检查实体ID是否有效
        /// @param id 实体ID
        /// @return 是否有效
        bool IsValidID(EntityID id) const noexcept;

        /// @brief 检查重新父子关系是否会创建循环依赖
        /// @param child 子实体ID
        /// @param newParent 新父实体ID
        /// @return 是否会创建循环依赖
        bool WouldCreateCycle(EntityID child, EntityID newParent) const noexcept;

        /// @brief 重建根实体缓存
        void RebuildRootCache();

        /// @brief 递归更新节点及其子节点的世界矩阵
        /// @param id 当前节点ID
        /// @param parentWorld 父节点的世界变换矩阵
        void UpdateNodeRecursive(EntityID id, const glm::mat4 &parentWorld);

    public:
        Scene() = default;
        ~Scene() = default;
        Scene(const Scene &) = delete;
        Scene &operator=(const Scene &) = delete;
        Scene(Scene &&) noexcept = default;
        Scene &operator=(Scene &&) noexcept = default;

        /// @brief 创建一个新实体, 返回其ID; 如果提供name参数则设置实体名称
        /// @param name 实体名称, 可选
        /// @return 新实体的ID
        [[nodiscard]] EntityID CreateEntity(std::string name = {});

        /// @brief 销毁指定实体及其所有子实体
        /// @param id 实体ID
        /// @return 是否成功销毁
        bool DestroyEntity(EntityID id);

        /// @brief 获取可修改的实体指针
        /// @param id 实体ID
        /// @return 实体指针, 如果ID无效则返回nullptr
        [[nodiscard]] Entity *GetEntity(EntityID id) noexcept;

        /// @brief 获取只读的实体指针
        /// @param id 实体ID
        /// @return 常量实体指针, 如果ID无效则返回nullptr
        [[nodiscard]] const Entity *GetEntity(EntityID id) const noexcept;

        /// @brief 检查场景是否包含指定实体
        /// @param id 实体ID
        /// @return 是否包含该实体
        [[nodiscard]] bool Contains(EntityID id) const noexcept { return IsValidID(id); };

        /// @brief 重新设置实体的父关系
        /// @param child 子实体ID
        /// @param newParent 新父实体ID, 如果为InvalidEntityID则表示设置为根实体
        /// @return 是否成功设置父关系, 失败可能是因为ID无效或会创建循环依赖
        bool Reparent(EntityID child, EntityID newParent);

        /// @brief 将实体从当前父关系中分离, 使其成为根实体
        /// @param child 子实体ID
        /// @return 是否成功分离, 失败可能是因为ID无效
        bool Detach(EntityID child);

        /// @brief 获取实体的父实体ID
        /// @param id 实体ID
        /// @return 父实体ID, 如果无父实体或ID无效则返回InvalidEntityID
        [[nodiscard]] EntityID GetParent(EntityID id) const noexcept;

        /// @brief 获取实体的子实体列表
        /// @param id 实体ID
        /// @return 子实体ID列表的span视图
        [[nodiscard]] std::span<const EntityID> GetChildren(EntityID id) const noexcept;

        /// @brief 更新所有实体的世界矩阵, 需要在每帧开始时调用以确保变换正确; 内部会根据父子关系递归更新
        void UpdateWorldMatrices();

        /// @brief 获取实体的世界变换矩阵
        /// @param id 实体ID
        /// @return 世界变换矩阵的常量引用
        [[nodiscard]] const glm::mat4 &GetWorldMatrix(EntityID id) const noexcept;

        /// @brief 获取实体的世界法线矩阵
        /// @param id 实体ID
        /// @return 世界法线矩阵的常量引用
        [[nodiscard]] const glm::mat3 &GetWorldNormalMatrix(EntityID id) const noexcept;

        //=====================================灯光系统=================================================

        /// @brief 获取灯光管理器，用于创建和管理光源
        /// @return 灯光管理器引用
        [[nodiscard]] class LightManager &GetLightManager() noexcept { return mLightManager; }

        /// @brief 获取灯光管理器，用于创建和管理光源
        /// @return 灯光管理器常量引用
        [[nodiscard]] const class LightManager &GetLightManager() const noexcept { return mLightManager; }

        // 兼容层
        [[nodiscard]] LightID CreateDirectionalLight(const DirectionalLight &light = {}) { return mLightManager.CreateLight(light); }
        bool RemoveDirectionalLight(LightID id) { return mLightManager.RemoveLight<DirectionalLight>(id); }
        [[nodiscard]] DirectionalLight *GetDirectionalLight(LightID id) noexcept { return mLightManager.GetLight<DirectionalLight>(id); }
        [[nodiscard]] const DirectionalLight *GetDirectionalLight(LightID id) const noexcept { return mLightManager.GetLight<DirectionalLight>(id); }
        [[nodiscard]] std::span<const DirectionalLight> GetDirectionalLights() const noexcept { return mLightManager.GetLights<DirectionalLight>(); }
        void ClearDirectionalLights() { mLightManager.ClearLights<DirectionalLight>(); }

        [[nodiscard]] LightID CreatePointLight(const PointLight &light = {}) { return mLightManager.CreateLight(light); }
        bool RemovePointLight(LightID id) { return mLightManager.RemoveLight<PointLight>(id); }
        [[nodiscard]] PointLight *GetPointLight(LightID id) noexcept { return mLightManager.GetLight<PointLight>(id); }
        [[nodiscard]] const PointLight *GetPointLight(LightID id) const noexcept { return mLightManager.GetLight<PointLight>(id); }
        [[nodiscard]] std::span<const PointLight> GetPointLights() const noexcept { return mLightManager.GetLights<PointLight>(); }
        void ClearPointLights() { mLightManager.ClearLights<PointLight>(); }

        [[nodiscard]] LightID CreateSpotLight(const SpotLight &light = {}) { return mLightManager.CreateLight(light); }
        bool RemoveSpotLight(LightID id) { return mLightManager.RemoveLight<SpotLight>(id); }
        [[nodiscard]] SpotLight *GetSpotLight(LightID id) noexcept { return mLightManager.GetLight<SpotLight>(id); }
        [[nodiscard]] const SpotLight *GetSpotLight(LightID id) const noexcept { return mLightManager.GetLight<SpotLight>(id); }
        [[nodiscard]] std::span<const SpotLight> GetSpotLights() const noexcept { return mLightManager.GetLights<SpotLight>(); }
        void ClearSpotLights() { return mLightManager.ClearLights<SpotLight>(); }

        //=====================================灯光系统=================================================

        /// @brief 遍历所有可渲染实体
        /// @tparam Fn 函数对象类型, 必须接受(EntityID, const Entity &, const glm::mat4 &, const glm::mat3 &)作为参数
        /// @param fn 要对每个可渲染实体调用的函数对象
        template <typename Fn>
        void ForEachRenderable(Fn &&fn) const
        {
            for (EntityID id = 0; id < static_cast<EntityID>(mEntities.size()); ++id)
            {
                if (!mEntities[id].has_value()) // 跳过空槽位
                    continue;

                const Entity &entity = mEntities[id].value();
                if (!entity.CanRender()) // 跳过不可渲染实体
                    continue;

                // 调用回调函数, 提供渲染所需的所有信息
                fn(id, entity, mNodes[id].__worldMatrix__, mNodes[id].__worldNormalMatrix__);
            }
        }
    };
}