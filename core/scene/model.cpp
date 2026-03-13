#include "model.hpp"
#include "scene.hpp"
#include <algorithm>
#include <string>
#include <utility>

namespace core
{
    Model::Model(std::string name) : mName(std::move(name)) {}

    ModelNodeID Model::AddNode(ModelNode node)
    {
        const ModelNodeID id = static_cast<ModelNodeID>(mNodes.size()); // 新节点ID为当前节点数量
        node.__parent__ = InvalidModelNodeID;                           // 设置父节点为无效ID(新节点默认为根节点)
        mNodes.push_back(std::move(node));                              // 将节点移动到节点数组中
        mRootNodes.push_back(id);                                       // 将新节点添加到根节点列表

        // 节点名称不为空且名称不存在于查找表中, 则添加到查找表
        if (!mNodes[id].__name__.empty() && mNodeLookup.find(mNodes[id].__name__) == mNodeLookup.end())
            mNodeLookup.emplace(mNodes[id].__name__, id);

        return id;
    }

    bool Model::AddChild(ModelNodeID parent, ModelNodeID child)
    {
        // 检查父节点和子节点ID是否有效, 且不能是同一个节点
        if (parent >= mNodes.size() || child >= mNodes.size() || parent == child)
            return false;

        ModelNode &parentNode = mNodes[parent];
        ModelNode &childNode = mNodes[child];

        // 如果子节点已经是该父节点的子节点直接返回
        if (childNode.__parent__ == parent)
            return true;

        // 如果子节点已经有其他父节点, 先从原父节点的子节点列表中移除
        if (childNode.__parent__ != InvalidModelNodeID)
        {
            auto &oldChildren = mNodes[childNode.__parent__].__children__;
            oldChildren.erase(std::remove(oldChildren.begin(), oldChildren.end(), child), oldChildren.end());
        }

        childNode.__parent__ = parent; // 设置子节点的新父节点
        // 如果子节点ID不在父节点的子节点列表, 添加到列表
        if (std::find(parentNode.__children__.begin(), parentNode.__children__.end(), child) == parentNode.__children__.end())
            parentNode.__children__.push_back(child);

        // 子节点不再是根节点, 从根节点列表中移除
        mRootNodes.erase(std::remove(mRootNodes.begin(), mRootNodes.end(), child), mRootNodes.end());
        return true;
    }

    ModelNodeID Model::FindNodeByName(std::string_view name) const noexcept
    {
        const auto it = mNodeLookup.find(std::string(name)); // 在查找表中搜索名称
        if (it == mNodeLookup.end())
            return InvalidModelNodeID;
        return it->second;
    }

    ModelInstance Model::Instantiate(Scene &scene, std::string_view instanceNamePrefix) const
    {
        ModelInstance instance{}; // 创建模型实例对象
        if (mNodes.empty())       // 模型没有节点返回空实例
            return instance;

        // 模型名称为空使用默认名称
        std::string rootName = mName.empty() ? "Model" : mName;
        // 如果有实例名称前缀使用前缀作为根名称
        if (!instanceNamePrefix.empty())
            rootName = std::string(instanceNamePrefix);
        rootName += "_Root"; // 添加"_Root"后缀

        instance.__rootEntity__ = scene.CreateEntity(rootName);           // 在场景中创建根实体
        instance.__nodeEntities__.assign(mNodes.size(), InvalidEntityID); // 预分配节点实体数组, 初始化为无效ID

        // 1) 创建所有节点对应的实体
        for (ModelNodeID nodeID = 0; nodeID < static_cast<ModelNodeID>(mNodes.size()); ++nodeID)
        {
            const ModelNode &node = mNodes[nodeID]; // 获取当前节点

            // 构建节点实体名称
            std::string nodeName = node.__name__.empty() ? ("Node" + std::to_string(nodeID)) : node.__name__;
            if (!instanceNamePrefix.empty())
                nodeName = std::string(instanceNamePrefix) + "/" + nodeName; // 添加前缀和路径分隔符

            const EntityID entityID = scene.CreateEntity(std::move(nodeName)); // 使用Node名在场景中创建实体
            instance.__nodeEntities__[nodeID] = entityID;                      // 记录Node对应的实体ID

            Entity *entity = scene.GetEntity(entityID); // 获取刚创建的实体
            if (!entity)
                continue;

            entity->GetTransform() = node.__localTransform__; // 设置实体的变换矩阵
            if (node.__mesh__)
                entity->SetMesh(node.__mesh__); // 设置实体的网格
            if (node.__material__)
                entity->SetMaterial(node.__material__); // 设置实体的材质
        }

        // 2) 建立实体间的父子关系(将Node中的父子关系转换到Entity)
        for (ModelNodeID nodeID = 0; nodeID < static_cast<ModelNodeID>(mNodes.size()); ++nodeID)
        {
            // 获取当前节点对应的实体ID并检查有效性
            const EntityID childEntity = instance.__nodeEntities__[nodeID];
            if (childEntity == InvalidEntityID)
                continue;

            // 获取当前节点在模型中的父节点ID
            const ModelNodeID parentNode = mNodes[nodeID].__parent__;
            // 1. 如果当前节点是根节点(没有父节点)
            if (parentNode == InvalidModelNodeID)
            {
                // 将这个实体设置为模型根实体的子节点
                scene.Reparent(childEntity, instance.__rootEntity__);
                continue;
            }

            // 2. 当前节点有父节点, 获取父节点对应的实体ID
            const EntityID parentEntity = instance.__nodeEntities__[parentNode];
            if (parentEntity != InvalidEntityID) // 如果父实体ID有效, 建立父子关系
                scene.Reparent(childEntity, parentEntity);
        }

        return instance;
    }

    void Model::RegisterBone(std::string boneName, const glm::mat4 &offsetMatrix)
    {
        if (boneName.empty()) // 骨骼名称为空
            return;

        if (mBoneLookup.find(boneName) != mBoneLookup.end()) // 骨骼已存在
            return;

        const uint32_t index = static_cast<uint32_t>(mBones.size()); // 新骨骼索引为当前骨骼数量
        mBoneLookup.emplace(boneName, index);                        // 添加到骨骼查找表

        // 创建骨骼信息
        BoneInfo info{};
        info.__name__ = std::move(boneName);
        info.__offsetMatrix__ = offsetMatrix;
        mBones.push_back(std::move(info));
    }

    void Model::ResolveBoneNodes()
    {
        for (auto &bone : mBones)
        {
            bone.__node__ = FindNodeByName(bone.__name__); // 通过骨骼名称查找对应的模型节点
            if (bone.__node__ != InvalidModelNodeID)
                mNodes[bone.__node__].__isSkeletonNode__ = true;
        }
    }
}