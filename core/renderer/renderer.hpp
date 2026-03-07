#pragma once
#include "camera/camera.hpp"
#include "renderer/frameData.hpp"
#include "renderer/renderQueue.hpp"
#include "renderer/renderStateCache.hpp"
#include "renderer/uniformBuffer.hpp"
#include "scene/scene.hpp"
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_set>

namespace core
{
    class Shader;
    class Material;
    class Mesh;

    // 记录每帧的渲染性能数据, 用于性能分析和调试
    struct RenderStats final
    {
        uint32_t __drawCalls__{0};    // 绘制调用次数(glDraw*调用)
        uint32_t __triangles__{0};    // 渲染的三角形总数
        uint32_t __programBinds__{0}; // 着色器程序绑定次数
        uint32_t __textureBinds__{0}; // 纹理绑定次数
    };

    /*
     * 功能模块:
     * 1. 管理GPU资源(UBO、状态缓存)
     * 2. 协调渲染管线(场景→队列→绘制)
     * 3. 性能优化(状态缓存、数据上传优化)
     * 4. 统计收集(性能监控)
     *
     * 设计模式:
     * - 单帧渲染架构(无多帧缓存)
     * - 立即模式渲染(每帧重建所有数据)
     * - 状态缓存优化(最小化GPU状态切换)
     * - UBO数据管理(高效GPU数据传输)
     */
    class Renderer final
    {
    private:
        static constexpr uint32_t FrameBlockBindingPoint = 0u; // 帧数据UBO绑定槽
        static constexpr uint32_t LightBlockBindingPoint = 1u; // 光照数据UBO绑定槽

        // ====== 渲染状态 ======
        glm::vec4 mClearColor{0.68f, 0.85f, 0.90f, 1.f}; // 默认天空蓝清屏颜色

        // ====== 渲染组件 ======
        RenderQueue mRenderQueue{};     // 渲染队列
        RenderStateCache mStateCache{}; // 状态缓存

        // ====== GPU资源 ======
        UniformBuffer mFrameBlockUBO{}; // 帧数据UBO(每帧更新)
        UniformBuffer mLightBlockUBO{}; // 光照数据UBO(变化时更新)

        // ====== 统计信息 ======
        RenderStats mStats{};     // 当前帧统计
        bool mInitialized{false}; // 初始化状态标记

        // ====== 缓存优化 ======
        uint32_t mCachedDirectionalCount{std::numeric_limits<uint32_t>::max()}; // 缓存方向光数量
        std::array<uint64_t, MaxDirectionalLights> mCachedLightVersions{};      // 缓存每个光源的版本
        std::unordered_set<GLuint> mProgramBlockBoundCache{};                   // 缓存已绑定UBO的程序

    private:
        /// @brief 确保渲染器已初始化
        /// @return  如果已初始化返回true, 否则尝试初始化
        bool EnsureInit();

        /// @brief 重置统计信息(在每帧渲染开始前调用, 清零所有计数器)
        void ResetStats();

        /// @brief 绑定程序的UBO块
        /// @param shader 要绑定UBO的着色器程序
        void BindProgramBlocks(const Shader &shader);

        /// @brief 上传帧数据到UBO
        /// @param camera 当前相机视图
        /// @param timeSec 当前时间(秒)
        void UploadFrameBlock(const Camera &camera, float timeSec);

        /// @brief 上传光照数据到UBO(只有光照数量或版本变化时才实际上传)
        /// @param directionalLights 方向光数组
        void UploadLightBlock(std::span<const DirectionalLight> directionalLights);

        /// @brief 应用材质参数
        /// @param material 要应用的材质
        /// @param shader 目标着色器程序
        void ApplyMaterial(const Material &material, const Shader &shader);

        /// @brief 绘制网格
        /// @param mesh 要绘制的网格
        void DrawMesh(const Mesh &mesh);

    public:
        Renderer() = default;
        ~Renderer() = default;

        /// @brief 初始化渲染器(创建UBO对象、设置OpenGL状态、初始化缓存)
        void Init();

        /// @brief 关闭渲染器, 清理渲染器资源
        void Shutdown();

        // ====== 状态设置 ======
        void SetClearColor(const glm::vec4 &clearColor) { mClearColor = clearColor; }
        const glm::vec4 &GetClearColor() const noexcept { return mClearColor; }

        /**
         * @brief 渲染一帧场景
         * @param scene 渲染场景
         * @param camera 观察相机
         * @param timeSec 当前时间
         *
         * 渲染管线流程:
         * 1. 清屏和初始化
         * 2. 更新场景世界矩阵
         * 3. 构建渲染队列(排序优化)
         * 4. 上传UBO数据(帧数据、光照数据)
         * 5. 遍历渲染队列执行绘制
         * 6. 收集统计信息
         *
         * 性能优化策略:
         * - 状态缓存最小化(RenderStateCache)
         * - 渲染队列排序优化(RenderQueue)
         * - UBO数据变化检测(智能上传)
         * - 材质版本缓存(避免重复设置)
         */
        void Render(Scene &scene, const Camera &camera, float timeSec = 0.f);

        /// @brief 获取当前帧的渲染统计信息
        /// @return 渲染统计信息结构体
        const RenderStats &GetStats() const noexcept { return mStats; }
    };
}