#pragma once
#include "buffer/frameBuffer.hpp"
#include "buffer/uniformBuffer.hpp"
#include "frameData.hpp"
#include "renderQueue.hpp"
#include "renderStateCache.hpp"
#include "renderOption.hpp"
#include "profiler.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_set>

namespace core
{
    class Camera;
    class DirectionalLight;
    class Material;
    class Mesh;
    class Shader;
    class EnvironmentMap;

    class RenderBackend final
    {
    private:
        static constexpr uint32_t FrameBlockBindingPoint = 0u; // 帧数据UBO绑定槽
        static constexpr uint32_t LightBlockBindingPoint = 1u; // 光照数据UBO绑定槽
        UniformBuffer mFrameBlockUBO{};                        // 帧数据UBO(每帧更新)
        UniformBuffer mLightBlockUBO{};                        // 光照数据UBO(变化时更新)

        RenderStateCache mStateCache{}; // 状态缓存避免冗余的OpenGL状态切换

        uint32_t mCachedDirectionalCount{std::numeric_limits<uint32_t>::max()};  // 缓存的定向光数量
        std::array<uint64_t, MaxDirectionalLights> mCachedDirectionalVersions{}; // 平行光源的版本号缓存
        uint32_t mCachedPointCount{std::numeric_limits<uint32_t>::max()};        // 缓存的点光源数量
        std::array<uint64_t, MaxPointLights> mCachedPointVersions{};             // 点光源的版本号缓存
        uint32_t mCachedSpotCount{std::numeric_limits<uint32_t>::max()};         // 缓存的聚光灯数量
        std::array<uint64_t, MaxSpotLights> mCachedSpotVersions{};               // 聚光灯的版本号缓存
        std::unordered_set<GLuint> mProgramBlockBoundCache{};                    // 已绑定UBO的着色器程序缓存

        bool mInitialized{false};
        glm::vec4 mClearColor{};

        RenderStateDesc mAppliedState{};// 已应用的渲染状态描述
        bool mHasAppliedState{false}; // 是否已应用渲染状态

        GLuint mFullscreenTriangleVAO{0}; // 全屏三角形VAO

    private:
        /// @brief 为着色器程序绑定uniform block到指定槽位
        /// @param shader 目标着色器
        void BindProgramBlocks(const Shader &shader);

        /// @brief 应用材质参数到着色器
        /// @param material 材质对象
        /// @param shader 目标着色器
        /// @param stats 渲染性能统计
        void ApplyMaterial(const Material &material, const Shader &shader, RenderProfiler &stats);

        /// @brief 绘制网格几何体
        /// @param mesh 网格对象
        /// @param stats 渲染性能统计
        void DrawMesh(const Mesh &mesh, RenderProfiler &stats);

        /// @brief 应用渲染状态描述
        /// @param state 渲染状态描述
        void ApplyRenderState(const RenderStateDesc &state);

        /// @brief 绘制可绘制项
        /// @param items 绘制项数组
        /// @param stats 渲染性能统计
        void DrawItems(std::span<const DrawItem> items, RenderProfiler &stats);

    public:
        /// @brief 初始化渲染后端, 创建UBO并设置基础OpenGL状态
        /// @return 初始化成功返回true
        bool Init();

        /// @brief 关闭渲染后端, 清理GPU资源
        void Shutdown();

        /// @brief 检查渲染后端是否已正确初始化
        /// @return 初始化状态
        bool IsInitialized() const noexcept { return mInitialized && mFrameBlockUBO.IsValid() && mLightBlockUBO.IsValid(); }

        /// @brief 开始渲染到指定目标
        /// @param target 帧缓冲对象(为nullptr时渲染到默认帧缓冲)
        /// @param clearColor 清除颜色
        /// @param clearDepth 是否清除深度缓冲
        /// @param clearStencil 是否清除模板缓冲
        void BeginRenderTarget(const FrameBuffer *target, bool clearColor = true, bool clearDepth = true, bool clearStencil = false);

        /// @brief 结束当前渲染目标
        void EndRenderTarget();

        /// @brief 上传帧数据到UBO
        /// @param camera 摄像机对象
        /// @param timeSec 当前时间(秒)
        void UploadFrameBlock(const Camera &camera, float timeSec);

        /// @brief 上传光照数据到UBO
        /// @param directionalLights 定向光源数组
        /// @param pointLights 点光源数组
        /// @param spotLights 聚光光源数组
        void UploadLightBlock(std::span<const DirectionalLight> directionalLights,
                              std::span<const PointLight> pointLights,
                              std::span<const SpotLight> spotLights);

        /// @brief 绘制不透明物体队列
        /// @param queue 渲染队列
        /// @param stats 渲染性能统计
        void DrawOpaqueQueue(const RenderQueue &queue, RenderProfiler &stats);

        /// @brief 绘制透明物体队列
        /// @param queue 渲染队列
        /// @param stats 渲染性能统计
        void DrawTransparentQueue(const RenderQueue &queue, RenderProfiler &stats);

        /// @brief 应用Pass级基线状态(不依赖材质, 保证每个pass入口确定性)
        void ApplyPassState(const RenderStateDesc &state);

        /// @brief 绘制全屏纹理
        /// @param shader 着色器程序
        /// @param colorTexture 颜色纹理
        /// @param stats 渲染性能统计
        void DrawFullscreenTexture(const Shader &shader, GLuint colorTexture, RenderProfiler &stats);

        /// @brief 绘制天空盒(支持cubemap和panorama)
        /// @param shader 着色器程序
        /// @param cube 天空盒立方体网格
        /// @param skybox 环境贴图(立方体贴图或全景图)
        /// @param intensity 天空盒强度
        /// @param stats 渲染性能统计
        void DrawSkybox(const Shader &shader, const Mesh &cube, const EnvironmentMap &skybox, float intensity, RenderProfiler &stats);

        void SetClearColor(const glm::vec4 &color) { mClearColor = color; }
        const glm::vec4 &GetClearColor() const noexcept { return mClearColor; }
    };
}