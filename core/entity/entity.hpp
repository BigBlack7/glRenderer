#pragma once
#include "camera/camera.hpp"
#include "geometry/mesh.hpp"
#include "geometry/transform.hpp"
#include "material/material.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace core
{
    using EntityID = uint32_t;
    constexpr EntityID InvalidEntityID = std::numeric_limits<EntityID>::max(); // 无效ID哨兵值: 用最大值表示"未设置/不存在"

    class Entity final
    {
    private:
        static constexpr uint8_t FlagActive = 1u << 0u;  // 第0位: active
        static constexpr uint8_t FlagVisible = 1u << 1u; // 第1位: visible

        EntityID mID{InvalidEntityID};
        std::string mName{};

        // 结构化组件
        Transform mTransform{};                // 本地TRS
        std::shared_ptr<Mesh> mMesh{};         // 网格资源(共享拥有)
        std::shared_ptr<Material> mMaterial{}; // 材质资源(共享拥有)

        // 关系信息由上层Scene维护, Entity仅保存轻量索引
        EntityID mParent{InvalidEntityID};
        std::vector<EntityID> mChildren{};

        // 初始默认状态: 激活 & 可见
        uint8_t mFlags{static_cast<uint8_t>(FlagActive | FlagVisible)};

    public:
        Entity() = default;
        explicit Entity(EntityID id, std::string name = {});

        // ===== ID =====
        EntityID GetID() const noexcept { return mID; }
        void SetID(EntityID id) noexcept { mID = id; }

        // ===== Name =====
        const std::string &GetName() const noexcept { return mName; }
        void SetName(std::string name) { mName = std::move(name); }

        // ===== Transform =====
        Transform &GetTransform() noexcept { return mTransform; }
        const Transform &GetTransform() const noexcept { return mTransform; }

        // ===== Mesh & Material =====
        void SetMesh(std::shared_ptr<Mesh> mesh) noexcept { mMesh = std::move(mesh); }
        void SetMaterial(std::shared_ptr<Material> material) noexcept { mMaterial = std::move(material); }
        const std::shared_ptr<Mesh> &GetMesh() const noexcept { return mMesh; }
        const std::shared_ptr<Material> &GetMaterial() const noexcept { return mMaterial; }

        // ===== Parent-Child =====
        /// @brief 设置父实体, 不修改父实体的子列表, 由上层Scene统一维护关系一致性
        /// @param parent 父实体ID, 可以设置为InvalidEntityID表示无父实体
        void SetParent(EntityID parent) noexcept;
        EntityID GetParent() const noexcept { return mParent; }

        /// @brief 添加子实体, 由上层Scene统一维护关系一致性
        /// @param child 子实体ID
        /// @return 是否添加成功
        bool AddChild(EntityID child);

        /// @brief 移除子实体, 由上层Scene统一维护关系一致性
        /// @param child 子实体ID
        /// @return 是否移除成功
        bool RemoveChild(EntityID child);
        std::span<const EntityID> GetChildren() const noexcept { return mChildren; }

        // ===== Flags =====
        /// @brief 设置实体是否激活, 不影响子实体
        /// @param active 是否激活
        void SetActive(bool active) noexcept;

        /// @brief 获取实体是否激活, 不考虑父实体状态
        /// @return 是否激活
        bool IsActive() const noexcept;

        /// @brief 设置实体是否可见, 不影响子实体
        /// @param visible 是否可见
        void SetVisible(bool visible) noexcept;

        /// @brief 获取实体是否可见, 不考虑父实体状态
        /// @return 是否可见
        bool IsVisible() const noexcept;

        /// @brief 判断实体是否可渲染, 需要满足: 激活 & 可见 & Mesh & Material & Shader
        /// @return 是否可渲染
        bool CanRender() const noexcept;

        // ===== Matrix =====
        /// @brief 构建世界矩阵, 需要递归乘上父实体的世界矩阵以获得全局变换
        /// @param parentWorld 父实体的世界矩阵, 默认单位矩阵
        /// @return 世界矩阵
        glm::mat4 BuildWorldMatrix(const glm::mat4 &parentWorld = glm::mat4(1.f)) const;

        /// @brief 构建法线矩阵, 需要递归乘上父实体的世界矩阵以获得全局变换
        /// @param parentWorld 父实体的世界矩阵, 默认单位矩阵
        /// @return 法线矩阵
        glm::mat3 BuildNormalMatrix(const glm::mat4 &parentWorld = glm::mat4(1.f)) const;

        /// @brief 绘制自身实体, 不递归子实体; 层级遍历由Scene/Renderer负责
        /// @param camera 相机对象, 用于获取视图矩阵和投影矩阵
        /// @param parentWorld 父实体的世界矩阵, 默认单位矩阵
        void Draw(const Camera &camera, const glm::mat4 &parentWorld = glm::mat4(1.f)) const;
    };
}