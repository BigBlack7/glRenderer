#include "renderQueue.hpp"
#include "geometry/mesh.hpp"
#include "material/material.hpp"
#include "scene/entity.hpp"
#include "shader/shader.hpp"
#include <algorithm>
#include <utility>

namespace core
{
    namespace detail
    {
        /// @brief 不透明物体排序比较函数
        /// @param a 渲染项A
        /// @param b 渲染项B
        /// @return 是否A在B之前
        /*
         * 排序优先级:
         * 1. Shader程序: 相同着色器的物体连续渲染, 避免glUseProgram调用
         * 2. Material材质: 相同材质的物体连续渲染, 减少uniform更新
         * 3. Mesh网格: 相同网格的物体连续渲染, 可能启用实例化渲染
         * 4. EntityID实体ID: 最后按ID排序, 确保确定性排序
         */
        bool CompareOpaque(const DrawItem &a, const DrawItem &b)
        {
            if (a.__shader__ != b.__shader__)
                return a.__shader__ < b.__shader__;
            if (a.__material__ != b.__material__)
                return a.__material__ < b.__material__;
            if (a.__mesh__ != b.__mesh__)
                return a.__mesh__ < b.__mesh__;
            return a.__entityID__ < b.__entityID__;
        }
    }

    /*
     * 渲染队列构建流程:
     * 1. 清空旧数据(避免重新分配内存)
     * 2. 遍历场景收集可渲染对象
     * 3. 验证数据完整性(跳过无效对象)
     * 4. 创建DrawItem并填充数据
     * 5. 按优化顺序排序
     *
     * 性能分析:
     * - 时间复杂度: O(nlogn)(主要为排序)
     * - 空间复杂度: O(n)(存储所有可渲染对象)
     * - 内存分配: 最小化(使用vector预分配)
     */
    void RenderQueue::Build(const Scene &scene)
    {
        // 清空旧数据
        mOpaqueItems.clear();

        // 遍历场景收集可渲染对象
        scene.ForEachRenderable([&](EntityID id, const Entity &entity, const glm::mat4 &world, const glm::mat3 &normal)
                                {
                                    // 获取并验证渲染所需组件
                                    const auto &mesh = entity.GetMesh();
                                    const auto &material = entity.GetMaterial();

                                    if (!mesh || !material) // 跳过无效实体
                                        return;

                                    const auto &shader = material->GetShader();
                                    if (!shader) // 跳过没有着色器的材质
                                        return;

                                    // 创建DrawItem
                                    DrawItem item;
                                    item.__entityID__ = id;
                                    item.__entity__ = &entity;
                                    item.__mesh__ = mesh.get();
                                    item.__material__ = material.get();
                                    item.__shader__ = shader.get();
                                    item.__world__ = world;
                                    item.__normal__ = normal;
                                    mOpaqueItems.push_back(std::move(item));
                                    // end
                                });

        // 按优化顺序排序, 最大化状态缓存命中率
        std::sort(mOpaqueItems.begin(), mOpaqueItems.end(), detail::CompareOpaque);
    }
}