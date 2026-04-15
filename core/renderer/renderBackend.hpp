#pragma once
#include "buffer/frameBuffer.hpp"
#include "buffer/uniformBuffer.hpp"
#include "frameData.hpp"
#include "renderQueue.hpp"
#include "renderStateCache.hpp"
#include "renderOption.hpp"
#include "buffer/instanceBuffer.hpp"
#include "utils/profiler.hpp"
#include "material/materialBinding.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_set>
#include <unordered_map>

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

        InstanceBuffer mInstanceModelBuffer{};  // 每实例 world matrix
        InstanceBuffer mInstanceNormalBuffer{}; // 每实例 normal matrix(3列vec4)

        std::unordered_set<GLuint> mInstancedLayoutCache{}; // 已配置实例布局的VAO缓存
        std::vector<glm::mat4> mInstanceModelScratch{};     // 上传前临时缓存
        std::vector<glm::vec4> mInstanceNormalScratch{};    // 每实例3个vec4

        uint32_t mCachedDirectionalCount{std::numeric_limits<uint32_t>::max()};     // 缓存的定向光数量
        std::array<uint64_t, MaxDirectionalLights> mCachedDirectionalVersions{};    // 平行光源的版本号缓存
        uint32_t mCachedPointCount{std::numeric_limits<uint32_t>::max()};           // 缓存的点光源数量
        std::array<uint64_t, MaxPointLights> mCachedPointVersions{};                // 点光源的版本号缓存
        uint32_t mCachedSpotCount{std::numeric_limits<uint32_t>::max()};            // 缓存的聚光灯数量
        std::array<uint64_t, MaxSpotLights> mCachedSpotVersions{};                  // 聚光灯的版本号缓存
        std::unordered_set<GLuint> mProgramBlockBoundCache{};                       // 已绑定UBO的着色器程序缓存
        std::unordered_map<GLuint, MaterialShaderBindings> mMaterialBindingCache{}; // 材质绑定缓存

        bool mInitialized{false};
        glm::vec4 mClearColor{};

        RenderStateDesc mAppliedState{}; // 已应用的渲染状态描述
        bool mHasAppliedState{false};    // 是否已应用渲染状态

        GLuint mFullscreenTriangleVAO{0}; // 全屏三角形VAO

        static constexpr uint32_t DirectionalShadowTextureUnit = 15u;      // 单张方向阴影贴图纹理单元
        static constexpr uint32_t DirectionalShadowArrayTextureUnit = 14u; // CSM方向阴影贴图数组纹理单元
        static constexpr uint32_t PointShadowCubeTextureUnit = 13u;        // 点光阴影立方体贴图纹理单元
        static constexpr uint32_t SpotShadowTextureUnit = 12u;             // 聚光阴影贴图纹理单元
        GLuint mDirectionalShadowTexture{0};                               // 单张方向阴影贴图句柄
        glm::mat4 mDirectionalLightSpaceVP{1.f};                           // 单张方向光VP矩阵

        GLuint mDirectionalShadowArrayTexture{0};                                                                             // CSM深度数组纹理句柄
        uint32_t mDirectionalCascadeCount{0};                                                                                 // CSM级联数量
        std::array<float, 4> mDirectionalCascadeSplits{0.f, 0.f, 0.f, 0.f};                                                   // CSM级联切分深度
        std::array<glm::mat4, 4> mDirectionalCascadeLightVPs{glm::mat4(1.f), glm::mat4(1.f), glm::mat4(1.f), glm::mat4(1.f)}; // CSM级联VP数组
        std::array<float, 4> mDirectionalCascadeUVScales{1.f, 1.f, 1.f, 1.f};                                                 // 每级使用的有效UV比例(近大远小)

        GLuint mPointShadowCubeTexture{0};       // 点光阴影立方体深度贴图
        glm::vec3 mPointShadowLightPos{0.f};     // 点光阴影光源位置
        float mPointShadowFarPlane{1.f};         // 点光阴影far
        float mPointShadowBiasConstant{0.0008f}; // 点光阴影常量偏移
        float mPointShadowBiasSlope{0.002f};     // 点光阴影坡度偏移

        GLuint mSpotShadowTexture{0};           // 聚光阴影深度贴图
        glm::mat4 mSpotLightSpaceVP{1.f};       // 聚光阴影视图投影矩阵
        float mSpotShadowBiasConstant{0.0008f}; // 聚光阴影常量偏移
        float mSpotShadowBiasSlope{0.002f};     // 聚光阴影坡度偏移

        static constexpr uint32_t IblIrradianceTextureUnit = 11u;
        static constexpr uint32_t IblPrefilterTextureUnit = 10u;
        static constexpr uint32_t IblBrdfLutTextureUnit = 9u;
        GLuint mIblIrradianceMap{0};
        GLuint mIblPrefilterMap{0};
        GLuint mIblBrdfLut{0};

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

        /// @brief 确保实例布局已配置
        /// @param mesh 网格对象
        void EnsureInstancedLayout(const Mesh &mesh);

        /// @brief 上传实例数据到缓冲区
        /// @param items 绘制项数组
        /// @return 是否上传成功
        bool UploadInstanceData(std::span<const DrawItem> items);

        /// @brief 绘制实例化网格几何体
        /// @param mesh 网格对象
        /// @param instanceCount 实例数量
        /// @param stats 渲染性能统计
        void DrawMeshInstanced(const Mesh &mesh, uint32_t instanceCount, RenderProfiler &stats);

        /// @brief 绘制实例化物体列表, 适用于大量实例共享同一Mesh/Material的情况, 减少DrawCall数量
        /// @param items 绘制项数组
        /// @param stats 渲染性能统计
        void DrawInstancedItems(std::span<const DrawItem> items, RenderProfiler &stats);

        /// @brief 获取材质绑定缓存
        /// @param shader 着色器对象
        /// @return 材质绑定缓存
        const MaterialShaderBindings &GetMaterialBindings(const Shader &shader);

        /// @brief 为当前shader应用全局阴影资源和矩阵
        /// @param shader 着色器对象
        /// @param stats 渲染性能统计
        void ApplyShadowGlobals(const Shader &shader, RenderProfiler &stats);

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

        /// @brief 绘制阴影深度队列(支持批次实例化)
        /// @param items 绘制项数组
        /// @param batches 批次信息(与items一致)
        /// @param shader 阴影着色器
        /// @param lightVP 光源VP矩阵
        /// @param stats 渲染性能统计
        void DrawShadowDepth(std::span<const DrawItem> items, std::span<const DrawBatch> batches, const Shader &shader, const glm::mat4 &lightVP, RenderProfiler &stats);

        /// @brief 应用Pass级基线状态(不依赖材质, 保证每个pass入口确定性)
        void ApplyPassState(const RenderStateDesc &state);

        /// @brief 设置方向光阴影图资源
        /// @param shadowTexture 阴影深度纹理
        /// @param lightSpaceVP 光空间VP矩阵
        void SetDirectionalShadow(GLuint shadowTexture, const glm::mat4 &lightSpaceVP);

        /// @brief 清理方向光阴影图资源
        void ClearDirectionalShadow();

        /// @brief 设置方向光CSM阴影图资源
        /// @param shadowArrayTexture 阴影数组纹理
        /// @param cascadeLightVPs 级联VP矩阵
        /// @param cascadeSplits 级联分割深度(视空间正向距离)
        /// @param cascadeUVScales 每级有效UV缩放(用于近远分辨率差异)
        /// @param cascadeCount 级联数量
        void SetDirectionalShadowCSM(GLuint shadowArrayTexture, std::span<const glm::mat4> cascadeLightVPs, std::span<const float> cascadeSplits, std::span<const float> cascadeUVScales, uint32_t cascadeCount);

        /// @brief 清理方向光CSM阴影图资源
        void ClearDirectionalShadowCSM();

        /// @brief 设置点光阴影图资源
        /// @param shadowCubeTexture 阴影立方体纹理
        /// @param lightPos 光源位置
        /// @param farPlane 远平面
        /// @param biasConstant 常量偏移
        /// @param biasSlope 坡度偏移
        void SetPointShadow(GLuint shadowCubeTexture, const glm::vec3 &lightPos, float farPlane, float biasConstant, float biasSlope);

        /// @brief 清理点光阴影图资源
        void ClearPointShadow();

        /// @brief 设置聚光阴影图资源
        /// @param shadowTexture 阴影深度纹理
        /// @param lightSpaceVP 光空间VP矩阵
        /// @param biasConstant 常量偏移
        /// @param biasSlope 坡度偏移
        void SetSpotShadow(GLuint shadowTexture, const glm::mat4 &lightSpaceVP, float biasConstant, float biasSlope);

        /// @brief 清理聚光阴影图资源
        void ClearSpotShadow();

        void SetIblMaps(GLuint irradianceMap, GLuint prefilterMap, GLuint brdfLut);
        void ClearIblMaps();

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