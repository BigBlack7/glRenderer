#pragma once
#include "geometry/transform.hpp"
#include "entity.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace core
{
    class Scene;
    class Mesh;
    class Material;

    using ModelNodeID = uint32_t;
    constexpr ModelNodeID InvalidModelNodeID = std::numeric_limits<ModelNodeID>::max();

    struct ModelNode final
    {
        std::string __name__{};
        ModelNodeID __parent__{InvalidModelNodeID}; // 父节点ID
        std::vector<ModelNodeID> __children__{};    // 子节点ID列表
        Transform __localTransform__{};             // 本地变换矩阵
        std::shared_ptr<Mesh> __mesh__{};           // 共享网格数据
        std::shared_ptr<Material> __material__{};   // 共享材质数据
        bool __isSkeletonNode__{false};             // 是否为骨骼节点
    };

    struct BoneInfo final
    {
        std::string __name__{};
        ModelNodeID __node__{InvalidModelNodeID}; // 对应的模型节点ID
        glm::mat4 __offsetMatrix__{1.f};          // 骨骼偏移
    };

    struct ModelInstance final
    {
        EntityID __rootEntity__{InvalidEntityID}; // 根实体ID
        std::vector<EntityID> __nodeEntities__{}; // 节点实体ID列表
    };

    class Model final
    {
    private:
        std::string mName{};
        std::vector<ModelNode> mNodes{};                            // 模型节点数组
        std::vector<ModelNodeID> mRootNodes{};                      // 根节点ID数组
        std::unordered_map<std::string, ModelNodeID> mNodeLookup{}; // 节点名称到ID的映射

        // TODO: 骨骼名 -> 节点索引 + 偏移矩阵
        std::vector<BoneInfo> mBones{};
        std::unordered_map<std::string, uint32_t> mBoneLookup{};

    public:
        explicit Model(std::string name = {});

        /// @brief 添加一个模型节点
        /// @param node 要添加的模型节点
        /// @return 新添加节点的ID
        ModelNodeID AddNode(ModelNode node);

        /// @brief 添加一个子节点到父节点
        /// @param parent 父节点ID
        /// @param child 子节点ID
        /// @return 是否添加成功
        bool AddChild(ModelNodeID parent, ModelNodeID child);

        /// @brief 根据节点名称查找节点ID
        /// @param name 节点名称
        /// @return 节点ID
        ModelNodeID FindNodeByName(std::string_view name) const noexcept;

        /// @brief 实例化模型到场景中, 创建对应的实体并设置变换、网格和材质
        /// @param scene 目标场景
        /// @param instanceNamePrefix 实例名称前缀
        /// @return 实例化后的模型实例
        ModelInstance Instantiate(Scene &scene, std::string_view instanceNamePrefix = {}) const;

        // TODO: 骨骼相关
        const std::vector<BoneInfo> &GetBones() const noexcept { return mBones; }

        /// @brief 注册骨骼信息, 包括骨骼名称和偏移矩阵
        /// @param boneName 骨骼名称
        /// @param offsetMatrix 骨骼偏移矩阵
        void RegisterBone(std::string boneName, const glm::mat4 &offsetMatrix);

        /// @brief 解析骨骼节点, 关联骨骼信息到模型节点
        void ResolveBoneNodes();

        const std::string &GetName() const noexcept { return mName; }
        const std::vector<ModelNode> &GetNodes() const noexcept { return mNodes; }
        const std::vector<ModelNodeID> &GetRootNodes() const noexcept { return mRootNodes; }
    };
}