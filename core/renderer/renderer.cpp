#include "renderer/renderer.hpp"
#include "geometry/mesh.hpp"
#include "material/material.hpp"
#include "shader/shader.hpp"
#include "texture/texture.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <limits>

namespace core
{
    bool Renderer::EnsureInit()
    {
        // 惰性初始化模式: 首次使用时自动初始化
        if (!mInitialized)
            Init();

        return mInitialized && mFrameBlockUBO.IsValid() && mLightBlockUBO.IsValid();
    }

    void Renderer::ResetStats()
    {
        mStats = {}; // 所有字段设为0
    }

    /*
     * - 每个程序只绑定一次UBO块
     * - 使用unordered_set快速查找
     * - 程序重链接后需要重新绑定
     * 绑定FrameBlock和LightBlock到预设槽位
     */
    void Renderer::BindProgramBlocks(const Shader &shader)
    {
        // 检查是否已经为当前程序绑定过UBO
        if (mProgramBlockBoundCache.find(shader.GetID()) != mProgramBlockBoundCache.end())
            return;

        shader.BindUniformBlock("FrameBlock", FrameBlockBindingPoint);
        shader.BindUniformBlock("LightBlock", LightBlockBindingPoint);
        mProgramBlockBoundCache.insert(shader.GetID()); // 记录该程序已经绑定过UBO
    }

    /*
     * 每帧必更新的数据:
     * - 视图矩阵(V)
     * - 投影矩阵(P)
     * - 相机位置(xyz)
     * - 当前时间(w分量)
     */
    void Renderer::UploadFrameBlock(const Camera &camera, float timeSec)
    {
        FrameBlockData frame{};
        frame.uV = camera.GetViewMatrix();
        frame.uP = camera.GetProjectionMatrix();
        frame.uViewPosTime = glm::vec4(camera.GetPosition(), timeSec);
        mFrameBlockUBO.SetData(static_cast<GLsizeiptr>(sizeof(FrameBlockData)), &frame);
    }

    /*
     * 1. 检查方向光数量是否变化
     * 2. 检查每个方向光的版本是否变化
     * 3. 只有真正变化时才上传数据
     * 避免不必要的GPU数据传输, 节省带宽
     */
    void Renderer::UploadLightBlock(std::span<const DirectionalLight> directionalLights)
    {
        LightBlockData lightBlock{}; // 创建光照数据结构
        std::array<uint64_t, MaxDirectionalLights> currentVersions{}; // 当前版本数组
        currentVersions.fill(0u);                                     // 初始化为0

        uint32_t activeCount = 0u; // 激活的方向光数量
        for (const auto &light : directionalLights)
        {
            if (activeCount >= MaxDirectionalLights) // 安全检查: 不超过最大支持数量
                break;
            if (!light.IsEnabled()) // 跳过禁用的光源
                continue;

            lightBlock.uDirectionalLights[activeCount].direction = glm::vec4(light.GetDirection(), 0.f);
            lightBlock.uDirectionalLights[activeCount].colorIntensity = glm::vec4(light.GetColor(), light.GetIntensity());
            currentVersions[activeCount] = light.GetVersion();
            ++activeCount;
        }

        lightBlock.uDirectionalLightMeta = glm::ivec4(static_cast<int>(activeCount), 0, 0, 0);

        // 变化检测
        const bool countChanged = (activeCount != mCachedDirectionalCount);
        const bool versionChanged = (currentVersions != mCachedLightVersions);
        if (!countChanged && !versionChanged) // 没有变化跳过上传
            return;

        // 有变化上传新数据到GPU
        mLightBlockUBO.SetData(static_cast<GLsizeiptr>(sizeof(LightBlockData)), &lightBlock);
        
        // 更新缓存值
        mCachedDirectionalCount = activeCount;
        mCachedLightVersions = currentVersions;
    }

    /*
     * 按顺序应用:
     * 1. 纹理绑定和采样器设置
     * 2. 材质特性标志
     * 3. 各种uniform参数(float、int、uint、vec3)
     */
    void Renderer::ApplyMaterial(const Material &material, const Shader &shader)
    {
        const auto &textures = material.GetTextures();
        const auto &samplerNames = material.GetTextureUniformNames();

        for (size_t i = 0; i < textures.size(); ++i)
        {
            const auto &tex = textures[i];
            if (!tex)
                continue;

            const uint32_t unit = static_cast<uint32_t>(i);
            if (mStateCache.BindTexture2D(unit, tex->GetID()))
                ++mStats.__textureBinds__;

            const std::string &samplerName = samplerNames[i];
            if (!samplerName.empty())
                shader.SetInt(samplerName, static_cast<int>(unit));
        }

        shader.SetUInt("uMaterialFlags", material.GetFeatureFlags());

        for (const auto &[name, value] : material.GetFloatParams())
            shader.SetFloat(name, value);
        for (const auto &[name, value] : material.GetIntParams())
            shader.SetInt(name, value);
        for (const auto &[name, value] : material.GetUIntParams())
            shader.SetUInt(name, value);
        for (const auto &[name, value] : material.GetVec3Params())
            shader.SetVec3(name, value);
    }

    void Renderer::DrawMesh(const Mesh &mesh)
    {
        if (mesh.GetVAO() == 0 || mesh.GetVertexCount() == 0)
            return;

        mStateCache.BindVertexArray(mesh.GetVAO());

        if (mesh.GetIndexCount() > 0)
        {
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.GetIndexCount()), GL_UNSIGNED_INT, nullptr);
            mStats.__triangles__ += mesh.GetIndexCount() / 3u;
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.GetVertexCount()));
            mStats.__triangles__ += mesh.GetVertexCount() / 3u;
        }

        ++mStats.__drawCalls__;
    }

    void Renderer::Init()
    {
        if (mInitialized) // 防止重复初始化
            return;

        // 创建帧数据UBO(96字节, 动态更新)
        const bool frameOk = mFrameBlockUBO.Create(static_cast<GLsizeiptr>(sizeof(FrameBlockData)), FrameBlockBindingPoint, GL_DYNAMIC_DRAW);

        // 创建光照数据UBO(160字节, 动态更新)
        const bool lightOk = mLightBlockUBO.Create(static_cast<GLsizeiptr>(sizeof(LightBlockData)), LightBlockBindingPoint, GL_DYNAMIC_DRAW);

        if (!frameOk || !lightOk) // 任何一个UBO创建失败都标记初始化失败
        {
            GL_CRITICAL("[Renderer] Init Failed: UBO Creation Failed");
            mInitialized = false;
            return;
        }

        // 设置OpenGL渲染状态
        glEnable(GL_DEPTH_TEST); // 启用深度测试
        glDepthFunc(GL_LESS);    // 深度小于时通过测试

        // 初始化各种缓存, 设置无效缓存值
        mStateCache.Reset();
        mCachedDirectionalCount = std::numeric_limits<uint32_t>::max();
        mCachedLightVersions.fill(std::numeric_limits<uint64_t>::max());
        mProgramBlockBoundCache.clear(); // 清空程序UBO绑定缓存

        mInitialized = true;
        GL_INFO("[Renderer] GLRenderer Init Succeeded");
    }

    /*
     * 资源释放顺序:
     * 1. UBO对象(GPU内存)
     * 2. 渲染队列(CPU内存)
     * 3. 程序绑定缓存(CPU内存)
     * 4. 状态缓存(重置状态)
     * 5. 标记未初始化
     */
    void Renderer::Shutdown()
    {
        mFrameBlockUBO = UniformBuffer{};
        mLightBlockUBO = UniformBuffer{};
        mRenderQueue.Clear();
        mProgramBlockBoundCache.clear();
        mStateCache.Reset();
        mInitialized = false;
    }

    void Renderer::Render(Scene &scene, const Camera &camera, float timeSec)
    {
        // 确保渲染器已初始化(惰性初始化)
        if (!EnsureInit())
            return;

        // 重置统计信息和清屏
        ResetStats();

        // 设置清屏颜色并执行清屏(颜色缓冲+深度缓冲)
        glClearColor(mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 准备场景数据
        scene.UpdateWorldMatrices();
        mRenderQueue.Build(scene);

        // 上传GPU数据
        UploadFrameBlock(camera, timeSec);
        UploadLightBlock(scene.GetDirectionalLights());

        // 渲染状态缓存变量
        const Material *lastMaterial = nullptr;
        uint64_t lastMaterialVersion = 0u;
        const Shader *lastShader = nullptr;

        for (const DrawItem &item : mRenderQueue.GetOpaqueItems())
        {
            // 数据完整性检查: 跳过无效项
            if (!item.__shader__ || !item.__material__ || !item.__mesh__)
                continue;

            const Shader &shader = *item.__shader__;
            const Material &material = *item.__material__;

            // 着色器程序切换检测
            if (mStateCache.UseProgram(shader.GetID()))
            {
                ++mStats.__programBinds__; // 统计程序绑定次数
                BindProgramBlocks(shader); // 绑定UBO块
                lastShader = &shader;      // 更新缓存
                lastMaterial = nullptr;    // 重置材质缓存
                lastMaterialVersion = 0u;  // 重置版本缓存
            }
            else if (lastShader != &shader) // 程序相同但指针不同
            {
                BindProgramBlocks(shader); // 重新绑定UBO块
                lastShader = &shader;
                lastMaterial = nullptr;
                lastMaterialVersion = 0u;
            }

            // 材质切换检测(程序变更或材质变更或版本变更)
            if (lastMaterial != &material || lastMaterialVersion != material.GetVersion())
            {
                ApplyMaterial(material, shader); // 应用新材质参数
                lastMaterial = &material;        // 更新缓存
                lastMaterialVersion = material.GetVersion(); // 更新版本缓存
            }

            // 设置变换矩阵(每物体必更新)
            shader.SetMat4("uM", item.__world__);
            shader.SetMat3("uN", item.__normal__);

            DrawMesh(*item.__mesh__);
        }
    }
}