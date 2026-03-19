#pragma once
#include "scene/scene.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace core
{
    class Camera;
    class Entity;
    class Mesh;
    class Material;
    class Shader;

    struct DrawItem final
    {
        EntityID __entityID__{InvalidEntityID};
        const Entity *__entity__{nullptr};
        const Mesh *__mesh__{nullptr};
        const Material *__material__{nullptr};
        const Shader *__shader__{nullptr};
        glm::mat4 __world__{1.f};
        glm::mat3 __normal__{1.f};
        float __distanceToCameraSq__{0.f};
    };

    struct DrawBatch final
    {
        uint32_t __start__{0};
        uint32_t __count__{0};
    };

    class RenderQueue final
    {
    private:
        std::vector<DrawItem> mOpaqueItems{}; // 不透明物体渲染队列(排序1)
        std::vector<DrawItem> mTransparentItems{}; // 透明物体渲染队列(排序2)
        std::vector<DrawBatch> mOpaqueBatches{};   // 同 shader/material/mesh 的连续批次

    public:
        /// @brief 遍历场景中的所有可渲染对象, 收集渲染数据, 并按优化顺序排序(当前排序规则：Shader → Material → Mesh → EntityID)
        /// @param scene 场景
        /// @param camera 相机
        void Build(const Scene &scene, const Camera &camera);

        /// @brief 清空队列, 避免内存重新分配的开销
        void Clear() { mOpaqueItems.clear(); mTransparentItems.clear(); mOpaqueBatches.clear(); }
        const std::vector<DrawItem> &GetOpaqueItems() const noexcept { return mOpaqueItems; }
        const std::vector<DrawItem> &GetTransparentItems() const noexcept { return mTransparentItems; }
        const std::vector<DrawBatch> &GetOpaqueBatches() const noexcept { return mOpaqueBatches; }
    };
}