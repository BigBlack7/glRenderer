#include "renderBackend.hpp"
#include "camera/camera.hpp"
#include "geometry/mesh.hpp"
#include "light/light.hpp"
#include "material/material.hpp"
#include "shader/shader.hpp"
#include "texture/texture.hpp"
#include "utils/logger.hpp"
#include <limits>

namespace core
{
    bool RenderBackend::Init()
    {
        if (mInitialized)
            return true;

        // 创建帧数据UBO - 存储视图/投影矩阵和摄像机位置
        const bool frameOk = mFrameBlockUBO.Create(static_cast<GLsizeiptr>(sizeof(FrameBlockData)), FrameBlockBindingPoint, GL_DYNAMIC_DRAW);
        // 创建光照数据UBO - 存储光源信息
        const bool lightOk = mLightBlockUBO.Create(static_cast<GLsizeiptr>(sizeof(LightBlockData)), LightBlockBindingPoint, GL_DYNAMIC_DRAW);

        if (!frameOk || !lightOk)
        {
            GL_CRITICAL("[RenderBackend] Init Failed: UBO Creation Failed");
            mInitialized = false;
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // 初始化状态缓存和光照缓存
        mStateCache.Reset();
        mCachedDirectionalCount = std::numeric_limits<uint32_t>::max();
        mCachedDirectionalVersions.fill(std::numeric_limits<uint64_t>::max());
        mCachedPointCount = std::numeric_limits<uint32_t>::max();
        mCachedPointVersions.fill(std::numeric_limits<uint64_t>::max());
        mCachedSpotCount = std::numeric_limits<uint32_t>::max();
        mCachedSpotVersions.fill(std::numeric_limits<uint64_t>::max());
        mProgramBlockBoundCache.clear();

        mInitialized = true;
        return true;
    }

    void RenderBackend::Shutdown()
    {
        mFrameBlockUBO = UniformBuffer{};
        mLightBlockUBO = UniformBuffer{};
        mStateCache.Reset();
        mProgramBlockBoundCache.clear();
        mInitialized = false;
    }

    void RenderBackend::BeginRenderTarget(const FrameBuffer *target, bool clearDepth)
    {
        if (target && target->IsValid())
            target->Bind(); // 绑定自定义帧缓冲
        else
            FrameBuffer::BindDefault(); // 绑定默认帧缓冲

        // 设置清除颜色并执行清除操作
        glClearColor(mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
        GLbitfield flags = GL_COLOR_BUFFER_BIT;
        if (clearDepth)
            flags |= GL_DEPTH_BUFFER_BIT;
        glClear(flags);
    }

    void RenderBackend::EndRenderTarget() {}

    void RenderBackend::BindProgramBlocks(const Shader &shader)
    {
        // 使用缓存检查避免重复绑定
        if (mProgramBlockBoundCache.find(shader.GetID()) != mProgramBlockBoundCache.end())
            return;

        // 绑定帧数据块到槽位0, 光照数据块到槽位1
        shader.BindUniformBlock("FrameBlock", FrameBlockBindingPoint);
        shader.BindUniformBlock("LightBlock", LightBlockBindingPoint);
        mProgramBlockBoundCache.insert(shader.GetID());
    }

    void RenderBackend::UploadFrameBlock(const Camera &camera, float timeSec)
    {
        FrameBlockData frame{};
        frame.uV = camera.GetViewMatrix();
        frame.uP = camera.GetProjectionMatrix();
        frame.uViewPosTime = glm::vec4(camera.GetPosition(), timeSec);
        mFrameBlockUBO.SetData(static_cast<GLsizeiptr>(sizeof(FrameBlockData)), &frame);
    }

    void RenderBackend::UploadLightBlock(std::span<const DirectionalLight> directionalLights,
                                         std::span<const PointLight> pointLights,
                                         std::span<const SpotLight> spotLights)
    {
        LightBlockData lightBlock{};

        // 收集有效方向光源数据
        std::array<uint64_t, MaxDirectionalLights> dirVersions{};
        dirVersions.fill(0u);
        uint32_t activeDirCount = 0u;
        for (const auto &light : directionalLights)
        {
            if (activeDirCount >= MaxDirectionalLights)
                break;
            if (!light.IsEnabled())
                continue;

            // 填充GPU结构体数据
            lightBlock.uDirectionalLights[activeDirCount].direction = glm::vec4(light.GetDirection(), 0.f);
            lightBlock.uDirectionalLights[activeDirCount].colorIntensity = glm::vec4(light.GetColor(), light.GetIntensity());
            dirVersions[activeDirCount] = light.GetVersion();
            ++activeDirCount;
        }
        lightBlock.uLightMeta.x = static_cast<int>(activeDirCount); // 存储有效方向光源数量为x

        // 收集有效点光源数据
        std::array<uint64_t, MaxPointLights> pointVersions{};
        pointVersions.fill(0u);
        uint32_t activePointCount = 0u;
        for (const auto &light : pointLights)
        {
            if (activePointCount >= MaxPointLights)
                break;
            if (!light.IsEnabled())
                continue;

            // 填充GPU结构体数据
            lightBlock.uPointLights[activePointCount].positionRange = glm::vec4(light.GetPosition(), light.GetRange());
            lightBlock.uPointLights[activePointCount].colorIntensity = glm::vec4(light.GetColor(), light.GetIntensity());
            lightBlock.uPointLights[activePointCount].attenuation = glm::vec4(light.GetAttenuation(), 0.f);
            pointVersions[activePointCount] = light.GetVersion();
            ++activePointCount;
        }
        lightBlock.uLightMeta.y = static_cast<int>(activePointCount); // 存储有效点光源数量为y

        // 收集有效聚光灯数据
        std::array<uint64_t, MaxSpotLights> spotVersions{};
        spotVersions.fill(0u);
        uint32_t activeSpotCount = 0u;
        for (const auto &light : spotLights)
        {
            if (activeSpotCount >= MaxSpotLights)
                break;
            if (!light.IsEnabled())
                continue;

            // 填充GPU结构体数据
            lightBlock.uSpotLights[activeSpotCount].positionRange = glm::vec4(light.GetPosition(), light.GetRange());
            lightBlock.uSpotLights[activeSpotCount].directionInnerCos = glm::vec4(light.GetDirection(), light.GetInnerCos());
            lightBlock.uSpotLights[activeSpotCount].colorIntensity = glm::vec4(light.GetColor(), light.GetIntensity());
            lightBlock.uSpotLights[activeSpotCount].attenuationOuterCos = glm::vec4(light.GetAttenuation(), light.GetOuterCos());
            spotVersions[activeSpotCount] = light.GetVersion();
            ++activeSpotCount;
        }
        lightBlock.uLightMeta.z = static_cast<int>(activeSpotCount); // 存储有效聚光灯数量为z

        // 只有数量和版本发生变化时才上传数据
        const bool anyChanged =
            (activeDirCount != mCachedDirectionalCount) || (dirVersions != mCachedDirectionalVersions) ||
            (activePointCount != mCachedPointCount) || (pointVersions != mCachedPointVersions) ||
            (activeSpotCount != mCachedSpotCount) || (spotVersions != mCachedSpotVersions);
        if (!anyChanged)
            return;

        // 上传新数据并更新缓存
        mLightBlockUBO.SetData(static_cast<GLsizeiptr>(sizeof(LightBlockData)), &lightBlock);
        mCachedDirectionalCount = activeDirCount;
        mCachedDirectionalVersions = dirVersions;
        mCachedPointCount = activePointCount;
        mCachedPointVersions = pointVersions;
        mCachedSpotCount = activeSpotCount;
        mCachedSpotVersions = spotVersions;
    }

    void RenderBackend::ApplyMaterial(const Material &material, const Shader &shader, RenderProfiler &stats)
    {
        const auto &textures = material.GetTextures();
        const auto &samplerNames = material.GetTextureUniformNames();

        // 绑定所有纹理并设置采样器uniform
        for (size_t i = 0; i < textures.size(); ++i)
        {
            const auto &tex = textures[i];
            if (!tex)
                continue;

            const uint32_t unit = static_cast<uint32_t>(i);

            // 使用状态缓存避免重复绑定
            if (mStateCache.BindTexture2D(unit, tex->GetID()))
                ++stats.__textureBinds__;

            const std::string &samplerName = samplerNames[i];
            if (!samplerName.empty())
                shader.SetInt(samplerName, static_cast<int>(unit));
        }

        // 设置材质特性标志
        shader.SetUInt("uMaterialFlags", material.GetFeatureFlags());

        // 应用各种材质参数
        for (const auto &[name, value] : material.GetFloatParams())
            shader.SetFloat(name, value);
        for (const auto &[name, value] : material.GetIntParams())
            shader.SetInt(name, value);
        for (const auto &[name, value] : material.GetUIntParams())
            shader.SetUInt(name, value);
        for (const auto &[name, value] : material.GetVec3Params())
            shader.SetVec3(name, value);
    }

    void RenderBackend::DrawMesh(const Mesh &mesh, RenderProfiler &stats)
    {
        if (mesh.GetVAO() == 0 || mesh.GetVertexCount() == 0)
            return;

        // 使用状态缓存优化绑定VAO
        mStateCache.BindVertexArray(mesh.GetVAO());

        // 根据是否有索引选择绘制方式
        if (mesh.GetIndexCount() > 0)
        {
            // 索引绘制
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.GetIndexCount()), GL_UNSIGNED_INT, nullptr);
            stats.__triangles__ += mesh.GetIndexCount() / 3u;
        }
        else
        {
            // 直接顶点绘制
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.GetVertexCount()));
            stats.__triangles__ += mesh.GetVertexCount() / 3u;
        }

        ++stats.__drawCalls__;
    }

    void RenderBackend::DrawOpaqueQueue(const RenderQueue &queue, RenderProfiler &stats)
    {
        const Material *lastMaterial = nullptr; // 缓存上一个材质
        uint64_t lastMaterialVersion = 0u;      // 缓存上一个材质版本
        const Shader *lastShader = nullptr;     // 缓存上一个着色器

        for (const DrawItem &item : queue.GetOpaqueItems())
        {
            if (!item.__shader__ || !item.__material__ || !item.__mesh__) // 跳过无效项
                continue;

            const Shader &shader = *item.__shader__;
            const Material &material = *item.__material__;

            if (mStateCache.UseProgram(shader.GetID())) // 着色器切换检测
            {
                ++stats.__programBinds__;  // 统计程序绑定
                BindProgramBlocks(shader); // 绑定UBO
                lastShader = &shader;
                lastMaterial = nullptr; // 重置材质缓存
                lastMaterialVersion = 0u;
            }
            else if (lastShader != &shader) // 同一着色器但缓存未命中(理论上不应该发生)
            {
                BindProgramBlocks(shader);
                lastShader = &shader;
                lastMaterial = nullptr;
                lastMaterialVersion = 0u;
            }

            // 材质切换检测, 避免重复应用相同材质
            if (lastMaterial != &material || lastMaterialVersion != material.GetVersion())
            {
                ApplyMaterial(material, shader, stats);
                lastMaterial = &material;
                lastMaterialVersion = material.GetVersion();
            }

            shader.SetMat4("uM", item.__world__);
            shader.SetMat3("uN", item.__normal__);
            DrawMesh(*item.__mesh__, stats);
        }
    }
}