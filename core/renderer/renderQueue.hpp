#pragma once
#include "scene/scene.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace core
{
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
    };

    class RenderQueue final
    {
    private:
        std::vector<DrawItem> mOpaqueItems{}; // 不透明物体渲染队列(已排序)

    public:
        /// @brief 遍历场景中的所有可渲染对象, 收集渲染数据, 并按优化顺序排序(当前排序规则：Shader → Material → Mesh → EntityID)
        /// @param scene 场景
        void Build(const Scene &scene);

        /// @brief 清空队列, 避免内存重新分配的开销
        void Clear() { mOpaqueItems.clear(); }
        const std::vector<DrawItem> &GetOpaqueItems() const noexcept { return mOpaqueItems; }
    };
}