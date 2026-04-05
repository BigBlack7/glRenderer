#include "shadowPass.hpp"
#include "camera/camera.hpp"
#include "light/light.hpp"
#include "renderer/renderBackend.hpp"
#include "shader/shader.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <span>
#include <utility>

namespace core
{
    namespace detail
    {
        /// @brief 从投影矩阵获得相机远近平面
        /// @param proj 投影矩阵
        /// @return 远近平面
        std::pair<float, float> ExtractPerspectiveNearFar(const glm::mat4 &proj)
        {
            const float a = proj[2][2];
            const float b = proj[3][2];
            const float nearPlane = b / (a - 1.f);
            const float farPlane = b / (a + 1.f);
            return {std::max(nearPlane, 0.01f), std::max(farPlane, nearPlane + 1.f)};
        }
    }

    bool ShadowPass::EnsureInit()
    {
        if (mShadowShader && mShadowShader->GetID() != 0 &&
            mPointShadowShader && mPointShadowShader->GetID() != 0 &&
            mShadowFBO != 0 && mShadowDepthTexture != 0 && mShadowDepthArrayTexture != 0 &&
            mPointShadowDepthCubeTexture != 0 && mSpotShadowDepthTexture != 0)
            return true;

        if (mInitTried)
            return false;

        mInitTried = true;
        mShadowShader = std::make_shared<Shader>("pass/shadow.vert", "pass/shadow.frag");
        mPointShadowShader = std::make_shared<Shader>("pass/shadowP.vert", "pass/shadowP.frag");
        if (!mShadowShader || mShadowShader->GetID() == 0 || !mPointShadowShader || mPointShadowShader->GetID() == 0)
            return false;

        return EnsureShadowResources();
    }

    void ShadowPass::Release() noexcept
    {
        if (mShadowDepthTexture != 0)
        {
            glDeleteTextures(1, &mShadowDepthTexture);
            mShadowDepthTexture = 0;
        }

        if (mShadowDepthArrayTexture != 0)
        {
            glDeleteTextures(1, &mShadowDepthArrayTexture);
            mShadowDepthArrayTexture = 0;
        }

        if (mPointShadowDepthCubeTexture != 0)
        {
            glDeleteTextures(1, &mPointShadowDepthCubeTexture);
            mPointShadowDepthCubeTexture = 0;
        }

        if (mSpotShadowDepthTexture != 0)
        {
            glDeleteTextures(1, &mSpotShadowDepthTexture);
            mSpotShadowDepthTexture = 0;
        }

        if (mShadowFBO != 0)
        {
            glDeleteFramebuffers(1, &mShadowFBO);
            mShadowFBO = 0;
        }
    }

    bool ShadowPass::EnsureShadowResources()
    {
        if (mShadowFBO != 0 && mShadowDepthTexture != 0 && mShadowDepthArrayTexture != 0 &&
            mPointShadowDepthCubeTexture != 0 && mSpotShadowDepthTexture != 0) // 检查是否已初始化
            return true;

        glGenFramebuffers(1, &mShadowFBO);      // 生成阴影帧缓冲区
        glGenTextures(1, &mShadowDepthTexture); // 生成阴影深度纹理
        glGenTextures(1, &mShadowDepthArrayTexture);
        glGenTextures(1, &mPointShadowDepthCubeTexture);
        glGenTextures(1, &mSpotShadowDepthTexture);

        if (mShadowFBO == 0 || mShadowDepthTexture == 0 || mShadowDepthArrayTexture == 0 ||
            mPointShadowDepthCubeTexture == 0 || mSpotShadowDepthTexture == 0) // 检查是否生成成功
        {
            Release();
            return false;
        }

        // 初始化阴影深度纹理
        glBindTexture(GL_TEXTURE_2D, mShadowDepthTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT32F,
            static_cast<GLsizei>(mShadowResolution),
            static_cast<GLsizei>(mShadowResolution),
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // 不进行平滑, 保持锐利的阴影边缘
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); // 超出[0,1]范围的纹理坐标将使用边框颜色
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        constexpr GLfloat borderColor[4] = {1.f, 1.f, 1.f, 1.f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); // 当纹理坐标超出范围时使用白色作为默认值, 即最大深度值

        glBindTexture(GL_TEXTURE_2D_ARRAY, mShadowDepthArrayTexture);
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_DEPTH_COMPONENT32F,
            static_cast<GLsizei>(mShadowResolution),
            static_cast<GLsizei>(mShadowResolution),
            static_cast<GLsizei>(mMaxCascadeCount),
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindTexture(GL_TEXTURE_CUBE_MAP, mPointShadowDepthCubeTexture);
        for (int i = 0; i < 6; ++i)
        {
            glTexImage2D(
                static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i),
                0,
                GL_DEPTH_COMPONENT32F,
                static_cast<GLsizei>(mPointShadowResolution),
                static_cast<GLsizei>(mPointShadowResolution),
                0,
                GL_DEPTH_COMPONENT,
                GL_FLOAT,
                nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, mSpotShadowDepthTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT32F,
            static_cast<GLsizei>(mSpotShadowResolution),
            static_cast<GLsizei>(mSpotShadowResolution),
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mShadowDepthTexture, 0); // 将深度纹理附加到FBO的深度附件点, 渲染到FBO时深度信息会自动写入这个纹理
        glDrawBuffer(GL_NONE);                                                                              // 禁用颜色绘制缓冲区
        glReadBuffer(GL_NONE);                                                                              // 禁用颜色读取缓冲区

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return status == GL_FRAMEBUFFER_COMPLETE;
    }

    const DirectionalLight *ShadowPass::SelectDirectionalLight(const FrameContext &ctx) const noexcept
    {
        if (!ctx.__scene__)
            return nullptr;

        for (const auto &light : ctx.__scene__->GetDirectionalLights())
        {
            if (!light.IsEnabled())
                continue;

            if (light.GetIntensity() <= 0.f)
                continue;

            const LightShadowSettings &shadow = light.GetShadowSettings();
            if (!shadow.mEnabled || shadow.mTechnique == ShadowTechnique::None)
                continue;

            return &light; // 返回第一个启用的定向光
        }

        return nullptr;
    }

    const PointLight *ShadowPass::SelectPointLight(const FrameContext &ctx) const noexcept
    {
        if (!ctx.__scene__)
            return nullptr;

        for (const auto &light : ctx.__scene__->GetPointLights())
        {
            if (!light.IsEnabled() || light.GetIntensity() <= 0.f)
                continue;

            const LightShadowSettings &shadow = light.GetShadowSettings();
            if (!shadow.mEnabled || shadow.mTechnique == ShadowTechnique::None)
                continue;

            return &light;
        }

        return nullptr;
    }

    const SpotLight *ShadowPass::SelectSpotLight(const FrameContext &ctx) const noexcept
    {
        if (!ctx.__scene__)
            return nullptr;

        for (const auto &light : ctx.__scene__->GetSpotLights())
        {
            if (!light.IsEnabled() || light.GetIntensity() <= 0.f)
                continue;

            const LightShadowSettings &shadow = light.GetShadowSettings();
            if (!shadow.mEnabled || shadow.mTechnique == ShadowTechnique::None)
                continue;

            return &light;
        }

        return nullptr;
    }

    glm::mat4 ShadowPass::BuildDirectionalLightVP(const FrameContext &ctx, const DirectionalLight &light) const
    {
        const auto &opaqueItems = ctx.__renderQueue__.GetOpaqueItems(); // 获取渲染队列中的所有不透明物体

        const glm::vec3 camPos = ctx.__camera__ ? ctx.__camera__->GetPosition() : glm::vec3(0.f, 0.f, 0.f); // 获取相机位置, 如果没有相机则使用原点
        glm::vec3 camForward{0.f, 0.f, -1.f};                                                               // 初始化相机前向向量(默认朝向负Z轴)
        // 如果存在相机计算真实的前向向量
        if (ctx.__camera__)
        {
            camForward = glm::normalize(glm::cross(ctx.__camera__->GetUp(), ctx.__camera__->GetRight()));
        }

        // 初始化世界空间边界框(用于包围所有需要阴影的物体)
        glm::vec3 worldMin(FLT_MAX);  // 最小边界点初始化为最大值
        glm::vec3 worldMax(-FLT_MAX); // 最大边界点初始化为最小值
        bool hasAnyPoint = false;     // 标记是否添加了任何点

        // 将一个点扩展到边界框中
        auto Expand = [&](const glm::vec3 &p)
        {
            worldMin = glm::min(worldMin, p);
            worldMax = glm::max(worldMax, p);
            hasAnyPoint = true;
        };

        // 遍历所有不透明物体将其世界位置加入边界框
        for (const DrawItem &item : opaqueItems)
            Expand(glm::vec3(item.__world__[3]));

        Expand(camPos);                     // 将相机位置加入边界框(确保相机附近的区域也被覆盖)
        Expand(camPos + camForward * 20.f); // 将相机前方20单位的位置加入边界框(覆盖相机视野)

        if (!hasAnyPoint) // 如果没有添加任何点设置默认边界框
        {
            worldMin = glm::vec3(-10.f, -10.f, -10.f);
            worldMax = glm::vec3(10.f, 10.f, 10.f);
        }

        const glm::vec3 extent = glm::max(worldMax - worldMin, glm::vec3(8.f));   // 计算边界框尺寸确保至少为8单位(避免过小)
        const float maxExtent = std::max(std::max(extent.x, extent.y), extent.z); // 找出最大维度, 用于计算填充距离
        const float padding = maxExtent * 0.35f + 8.f;                            // 计算填充距离(35%的最大维度 + 8单位), 用于包围盒扩展

        // 扩展边界框(添加填充, 确保阴影不会因为边缘物体而突然消失)
        worldMin -= glm::vec3(padding);
        worldMax += glm::vec3(padding);

        const glm::vec3 center = (worldMin + worldMax) * 0.5f; // 边界框中心点
        const glm::vec3 lightDir = glm::normalize(light.GetDirection());
        const glm::vec3 up = (std::abs(glm::dot(lightDir, glm::vec3(0.f, 1.f, 0.f))) > 0.98f) ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f); // 光源相机的上向量(避免光源方向接近垂直时出现数值问题)

        const float eyeDistance = glm::length(worldMax - worldMin) + 20.f; // 计算光源相机的视点距离(确保阴影覆盖所有物体)
        const glm::vec3 eye = center - lightDir * eyeDistance;             // 光源相机的视点位置

        const glm::mat4 lightView = glm::lookAt(eye, center, up); // 光源相机的视图矩阵

        // 定义边界框的8个角点
        std::array<glm::vec3, 8> corners{
            glm::vec3(worldMin.x, worldMin.y, worldMin.z),
            glm::vec3(worldMax.x, worldMin.y, worldMin.z),
            glm::vec3(worldMin.x, worldMax.y, worldMin.z),
            glm::vec3(worldMax.x, worldMax.y, worldMin.z),
            glm::vec3(worldMin.x, worldMin.y, worldMax.z),
            glm::vec3(worldMax.x, worldMin.y, worldMax.z),
            glm::vec3(worldMin.x, worldMax.y, worldMax.z),
            glm::vec3(worldMax.x, worldMax.y, worldMax.z)};

        // 初始化光源视图空间的边界
        glm::vec3 lightMin(FLT_MAX);
        glm::vec3 lightMax(-FLT_MAX);

        // 将所有角点转换到光源视图空间, 计算新的边界
        for (const glm::vec3 &corner : corners)
        {
            const glm::vec3 ls = glm::vec3(lightView * glm::vec4(corner, 1.f));
            lightMin = glm::min(lightMin, ls);
            lightMax = glm::max(lightMax, ls);
        }

        constexpr float zPadding = 20.f;                                          // 定义Z轴填充距离(确保阴影不会因为深度精度问题而消失)
        const float nearPlane = std::max(0.1f, -lightMax.z - zPadding);           // 计算近平面(确保不小于0.1, 避免裁剪平面问题)
        const float farPlane = std::max(nearPlane + 1.f, -lightMin.z + zPadding); // 计算远平面(确保比近平面大至少1单位)

        // 构建正交投影矩阵(确保覆盖场景边界框, 不浪费阴影贴图分辨率)
        const glm::mat4 lightProj = glm::ortho(
            lightMin.x, lightMax.x,
            lightMin.y, lightMax.y,
            nearPlane, farPlane);

        return lightProj * lightView;
    }

    std::array<float, 4> ShadowPass::BuildCascadeSplits(const FrameContext &ctx, const DirectionalLight &light, uint32_t &cascadeCount) const
    {
        const LightShadowSettings &shadow = light.GetShadowSettings();
        cascadeCount = std::clamp<uint32_t>(shadow.mCascadeCount, 2u, mMaxCascadeCount); // 将设置的级联数限制到 [2, mMaxCascadeCount]

        // 默认参数
        float nearDist = 0.1f;
        float farDist = 1000.f;
        if (ctx.__camera__) // 使用相机远近平面
        {
            const auto nearFar = detail::ExtractPerspectiveNearFar(ctx.__camera__->GetProjectionMatrix());
            nearDist = nearFar.first;
            farDist = nearFar.second;
        }

        const float exponent = std::max(shadow.mCascadeSplitExponent, 1.1f); // 指数权重, 用于控制级联分布(最小值1.1以避免退化为线性以下)

        std::array<float, 4> splits{farDist, farDist, farDist, farDist}; // 默认填充远平面
        for (uint32_t i = 0; i < cascadeCount; ++i)
        {
            const float p = static_cast<float>(i + 1u) / static_cast<float>(cascadeCount); // p 从 1/n 到 1
            const float t = std::pow(p, exponent);                                         // 将线性位置 p 进行指数映射以偏向近端或远端
            splits[i] = nearDist + (farDist - nearDist) * t;                               // 将 [near,far] 区间按 t 映射得到该级联的远端深度(以相机空间深度度量)
        }
        return splits;
    }

    std::array<glm::mat4, 4> ShadowPass::BuildCascadeLightVPs(const FrameContext &ctx, const DirectionalLight &light, const std::array<float, 4> &cascadeSplits, uint32_t cascadeCount) const
    {
        std::array<glm::mat4, 4> cascadeVPs{glm::mat4(1.f), glm::mat4(1.f), glm::mat4(1.f), glm::mat4(1.f)};
        if (!ctx.__camera__) // 无相机使用默认值
            return cascadeVPs;

        const glm::mat4 view = ctx.__camera__->GetViewMatrix();
        const glm::mat4 proj = ctx.__camera__->GetProjectionMatrix();
        const glm::mat4 invViewProj = glm::inverse(proj * view); // 逆视投影矩阵, 用于将 NDC 空间坐标转换回世界空间

        float nearDist = 0.1f;
        float farDist = 1000.f;
        const auto nearFar = detail::ExtractPerspectiveNearFar(proj);
        nearDist = nearFar.first;
        farDist = nearFar.second;

        std::array<glm::vec3, 4> nearCorners{}; // 存放视锥近平面四个角的世界坐标
        std::array<glm::vec3, 4> farCorners{};  // 存放视锥面远平面四个角的世界坐标
        // NDC 屏幕四个角的 x,y 值 (左下, 右下, 左上, 右上)
        const std::array<glm::vec2, 4> ndcXY = {
            glm::vec2(-1.f, -1.f),
            glm::vec2(1.f, -1.f),
            glm::vec2(-1.f, 1.f),
            glm::vec2(1.f, 1.f)};

        // 将 NDC 上的角点(near=-1, far=1)通过逆视投影转换到世界空间
        for (uint32_t i = 0; i < 4u; ++i)
        {
            glm::vec4 nearWS = invViewProj * glm::vec4(ndcXY[i], -1.f, 1.f); // NDC (x,y,-1,1) -> 世界空间齐次坐标
            nearWS /= nearWS.w;                                              // 齐次除法得到真实世界坐标
            nearCorners[i] = glm::vec3(nearWS);                              // 存储近平面角点的 xyz

            glm::vec4 farWS = invViewProj * glm::vec4(ndcXY[i], 1.f, 1.f); // NDC (x,y,1,1) -> 世界空间(远平面)
            farWS /= farWS.w;                                              // 齐次除法
            farCorners[i] = glm::vec3(farWS);                              // 存储远平面角点
        }

        const glm::vec3 lightDir = glm::normalize(light.GetDirection());
        const glm::vec3 up = (std::abs(glm::dot(lightDir, glm::vec3(0.f, 1.f, 0.f))) > 0.98f) ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f); // 当光方向几乎与世界 up (y) 平行时改用 z 轴避免数值不稳定

        float prevSplitDist = nearDist;                               // 上一个分割的起始深度, 初始为近裁剪面
        for (uint32_t cascade = 0; cascade < cascadeCount; ++cascade) // 为每个级联计算对应的视锥切片并生成光空间 VP 矩阵
        {
            // 将上一个分割距离映射到 [0, 1] 相对于 near/far
            const float splitNear = (prevSplitDist - nearDist) / std::max(farDist - nearDist, 0.001f);
            const float splitFar = (cascadeSplits[cascade] - nearDist) / std::max(farDist - nearDist, 0.001f);
            prevSplitDist = cascadeSplits[cascade]; // 更新 prevSplitDist 以便下一级联使用

            std::array<glm::vec3, 8> corners{}; // 该级联视锥切片的 8 个世界空间角点(近平面4点 + 远平面4点)
            for (uint32_t i = 0; i < 4u; ++i)   // 对每个屏幕角点沿 near->far 边插值得到该切片的角点
            {
                const glm::vec3 edge = farCorners[i] - nearCorners[i]; // 从近角到远角的向量
                corners[i] = nearCorners[i] + edge * splitNear;        // 该切片近平面角点(沿边按 splitNear 比例)
                corners[i + 4u] = nearCorners[i] + edge * splitFar;    // 该切片远平面角点
            }

            glm::vec3 frustumCenter(0.f); // 计算切片的质心, 用于构造光相机位置/镜头范围
            for (const glm::vec3 &corner : corners)
                frustumCenter += corner;
            frustumCenter /= 8.f; // 取平均得到中心点

            float radius = 0.f; // 用球体半径包围切片, 用于正交投影半宽
            for (const glm::vec3 &corner : corners)
                radius = std::max(radius, glm::length(corner - frustumCenter)); // 半径为最大角点到中心的距离

            // 计算在世界空间中一个 shadow map texel 的边长: 以包围球直径作为正交投影宽度来估算
            const float texelWorld = (2.f * radius) / static_cast<float>(mShadowResolution);
            if (texelWorld > 1e-5f)
                radius = std::ceil(radius / texelWorld) * texelWorld; // 将半径对齐到 texel 网格, 以减少随相机移动产生的抖动/闪烁

            // 将光源相机放在中心的光反方向一定距离处, 距离为直径 + 50 的额外偏移, 用于包含投射体并留有余量
            const glm::vec3 eye = frustumCenter - lightDir * (radius * 2.f + 50.f);
            glm::mat4 lightView = glm::lookAt(eye, frustumCenter, up); // 生成光空间视图矩阵

            // Texel Snapping: 将光空间中心吸附到texel网格, 减少抖动
            const glm::vec4 centerLS4 = lightView * glm::vec4(frustumCenter, 1.f); // 将中心变换到光空间
            glm::vec2 centerLS(centerLS4.x, centerLS4.y);                          // 取 x,y 作为水平/垂直中心
            if (texelWorld > 1e-5f)
                centerLS = glm::round(centerLS / texelWorld) * texelWorld;                    // 将中心坐标四舍五入到最近的 texel 网格位置
            const glm::vec2 offsetXY = centerLS - glm::vec2(centerLS4.x, centerLS4.y);        // 计算需要的偏移量以对齐到 texel
            lightView = glm::translate(glm::mat4(1.f), glm::vec3(offsetXY, 0.f)) * lightView; // 将光视图平移 offset, 使投影在 shadow map 上对齐到 texel 网格来减少抖动

            glm::vec3 lightMin(FLT_MAX);
            glm::vec3 lightMax(-FLT_MAX);
            for (const glm::vec3 &corner : corners)
            {
                const glm::vec3 ls = glm::vec3(lightView * glm::vec4(corner, 1.f));
                lightMin = glm::min(lightMin, ls);
                lightMax = glm::max(lightMax, ls);
            }

            // 在深度方向(光空间 z)向更小方向扩展, 增加前后余量
            lightMin.z -= 80.f;
            lightMax.z += 80.f;

            // 由于光空间 z 方向的符号约定(视图空间中前方通常为 -z), 这里对 z 做了符号取反以匹配 near/far 的正负关系
            const glm::mat4 lightProj = glm::ortho(lightMin.x, lightMax.x, lightMin.y, lightMax.y, -lightMax.z, -lightMin.z);
            cascadeVPs[cascade] = lightProj * lightView;
        }

        return cascadeVPs;
    }

    std::array<float, 4> ShadowPass::BuildCascadeUVScales(const DirectionalLight &light, uint32_t cascadeCount) const
    {
        const LightShadowSettings &shadow = light.GetShadowSettings();
        // 远端级联分辨率缩放因子, 限制在 [0.2, 1.0] 之间以避免过低或大于1
        const float farScale = std::clamp(shadow.mCascadeFarResolutionScale, 0.2f, 1.f);

        std::array<float, 4> scales{1.f, 1.f, 1.f, 1.f};
        if (cascadeCount <= 1u) // 若只有一个或更少级联直接返回默认值
            return scales;

        for (uint32_t i = 0; i < cascadeCount; ++i)
        {
            // 根据级联索引插值出每级的缩放: 索引越大越接近远端, 缩放越接近 farScale
            const float t = static_cast<float>(i) / static_cast<float>(cascadeCount - 1u);
            // t 在 [0,1] 内均匀分布: 0 -> 最近级联，1 -> 最远级联
            scales[i] = std::pow(farScale, t); // 近级联=1, 远级联逐步降低
        }
        return scales; // 每级级联的 UV 缩放比例
    }

    std::array<glm::mat4, 6> ShadowPass::BuildPointLightVPs(const PointLight &light) const
    {
        const glm::vec3 pos = light.GetPosition();
        const float zNear = 0.1f;
        const float zFar = std::max(light.GetRange(), zNear + 0.1f);
        const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, zNear, zFar);

        return {
            proj * glm::lookAt(pos, pos + glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f)),
            proj * glm::lookAt(pos, pos + glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f)),
            proj * glm::lookAt(pos, pos + glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f)),
            proj * glm::lookAt(pos, pos + glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f)),
            proj * glm::lookAt(pos, pos + glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f)),
            proj * glm::lookAt(pos, pos + glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f))};
    }

    glm::mat4 ShadowPass::BuildSpotLightVP(const SpotLight &light) const
    {
        const glm::vec3 pos = light.GetPosition();
        const glm::vec3 dir = glm::normalize(light.GetDirection());
        const float outer = glm::degrees(glm::acos(glm::clamp(light.GetOuterCos(), -1.f, 1.f)));
        const float fov = glm::clamp(outer * 2.f, 5.f, 175.f);
        const float zNear = 0.1f;
        const float zFar = std::max(light.GetRange(), zNear + 0.1f);
        const glm::vec3 up = (std::abs(glm::dot(dir, glm::vec3(0.f, 1.f, 0.f))) > 0.98f) ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);

        const glm::mat4 view = glm::lookAt(pos, pos + dir, up);
        const glm::mat4 proj = glm::perspective(glm::radians(fov), 1.f, zNear, zFar);
        return proj * view;
    }

    ShadowPass::~ShadowPass()
    {
        Release();
    }

    void ShadowPass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        ctx.__shadow__ = FrameContext::ShadowFrameData{}; // 初始化阴影帧数据
        backend.ClearDirectionalShadow();
        backend.ClearDirectionalShadowCSM();
        backend.ClearPointShadow();
        backend.ClearSpotShadow();

        if (!ctx.__scene__ || !ctx.__camera__)
            return;

        if (!EnsureInit())
            return;

        RenderStateDesc shadowState = MakeOpaqueState();
        shadowState.mDepth.mDepthTest = true;
        shadowState.mDepth.mDepthFunc = CompareOp::Less;
        shadowState.mDepth.mDepthWrite = true;
        shadowState.mStencil.mStencilTest = false;
        shadowState.mBlend.mBlend = false;

        // 避免shadow acne(阴影伪影)
        shadowState.mRaster.mFaceCull = true;
        shadowState.mRaster.mCullFace = CullMode::Front;
        shadowState.mRaster.mColorWriteR = false;
        shadowState.mRaster.mColorWriteG = false;
        shadowState.mRaster.mColorWriteB = false;
        shadowState.mRaster.mColorWriteA = false;

        shadowState.mPolygonOffset.mPolygonOffset = true;
        shadowState.mPolygonOffset.mPolygonOffsetType = PolygonOffsetMode::Fill;
        shadowState.mPolygonOffset.mFactor = 2.f;
        shadowState.mPolygonOffset.mUnit = 4.f;

        backend.ApplyPassState(shadowState);

        const DirectionalLight *light = SelectDirectionalLight(ctx);
        if (light)
        {
            const LightShadowSettings &shadow = light->GetShadowSettings();
            if (shadow.mTechnique == ShadowTechnique::CSM)
            {
                uint32_t cascadeCount = 0u;
                const std::array<float, 4> cascadeSplits = BuildCascadeSplits(ctx, *light, cascadeCount);
                const std::array<glm::mat4, 4> cascadeVPs = BuildCascadeLightVPs(ctx, *light, cascadeSplits, cascadeCount);
                const std::array<float, 4> cascadeUVScales = BuildCascadeUVScales(*light, cascadeCount);

                glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);

                for (uint32_t cascade = 0; cascade < cascadeCount; ++cascade)
                {
                    const float scale = std::clamp(cascadeUVScales[cascade], 0.1f, 1.f);
                    const GLsizei vpSize = static_cast<GLsizei>(std::max(64.f, static_cast<float>(mShadowResolution) * scale));

                    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mShadowDepthArrayTexture, 0, static_cast<GLint>(cascade));
                    glViewport(0, 0, vpSize, vpSize);
                    glClear(GL_DEPTH_BUFFER_BIT);

                    backend.DrawShadowDepth(
                        ctx.__renderQueue__.GetOpaqueItems(),
                        ctx.__renderQueue__.GetOpaqueBatches(),
                        *mShadowShader,
                        cascadeVPs[cascade],
                        ctx.__stats__);
                }

                backend.SetDirectionalShadowCSM(
                    mShadowDepthArrayTexture,
                    std::span<const glm::mat4>(cascadeVPs.data(), cascadeCount),
                    std::span<const float>(cascadeSplits.data(), cascadeCount),
                    std::span<const float>(cascadeUVScales.data(), cascadeCount),
                    cascadeCount);

                ctx.__shadow__.__hasDirectionalShadow__ = true;
                ctx.__shadow__.__directionalShadowTexture__ = mShadowDepthArrayTexture;
                ctx.__shadow__.__directionalLightVP__ = cascadeVPs[0];
                ctx.__shadow__.__directionalShadowResolution__ = mShadowResolution;
            }
            else
            {
                const glm::mat4 lightVP = BuildDirectionalLightVP(ctx, *light);

                glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);
                glViewport(0, 0, static_cast<GLsizei>(mShadowResolution), static_cast<GLsizei>(mShadowResolution));
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mShadowDepthTexture, 0);
                glClear(GL_DEPTH_BUFFER_BIT);

                backend.DrawShadowDepth(
                    ctx.__renderQueue__.GetOpaqueItems(),
                    ctx.__renderQueue__.GetOpaqueBatches(),
                    *mShadowShader,
                    lightVP,
                    ctx.__stats__);

                ctx.__shadow__.__hasDirectionalShadow__ = true;
                ctx.__shadow__.__directionalShadowTexture__ = mShadowDepthTexture;
                ctx.__shadow__.__directionalLightVP__ = lightVP;
                ctx.__shadow__.__directionalShadowResolution__ = mShadowResolution;
                backend.SetDirectionalShadow(mShadowDepthTexture, lightVP);
            }
        }

        if (const PointLight *pointLight = SelectPointLight(ctx); pointLight)
        {
            const std::array<glm::mat4, 6> pointVPs = BuildPointLightVPs(*pointLight);
            glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);
            glViewport(0, 0, static_cast<GLsizei>(mPointShadowResolution), static_cast<GLsizei>(mPointShadowResolution));
            glUseProgram(mPointShadowShader->GetID());

            mPointShadowShader->SetVec3Optional("uLightPos", pointLight->GetPosition());
            mPointShadowShader->SetFloatOptional("uFarPlane", std::max(pointLight->GetRange(), 0.1f));

            for (int face = 0; face < 6; ++face)
            {
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face),
                    mPointShadowDepthCubeTexture,
                    0);
                glClear(GL_DEPTH_BUFFER_BIT);

                backend.DrawShadowDepth(
                    ctx.__renderQueue__.GetOpaqueItems(),
                    ctx.__renderQueue__.GetOpaqueBatches(),
                    *mPointShadowShader,
                    pointVPs[face],
                    ctx.__stats__);
            }

            const LightShadowSettings &shadow = pointLight->GetShadowSettings();
            backend.SetPointShadow(
                mPointShadowDepthCubeTexture,
                pointLight->GetPosition(),
                std::max(pointLight->GetRange(), 0.1f),
                shadow.mBiasConstant,
                shadow.mBiasSlope);
        }

        if (const SpotLight *spotLight = SelectSpotLight(ctx); spotLight)
        {
            const glm::mat4 spotVP = BuildSpotLightVP(*spotLight);
            glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);
            glViewport(0, 0, static_cast<GLsizei>(mSpotShadowResolution), static_cast<GLsizei>(mSpotShadowResolution));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mSpotShadowDepthTexture, 0);
            glClear(GL_DEPTH_BUFFER_BIT);

            backend.DrawShadowDepth(
                ctx.__renderQueue__.GetOpaqueItems(),
                ctx.__renderQueue__.GetOpaqueBatches(),
                *mShadowShader,
                spotVP,
                ctx.__stats__);

            const LightShadowSettings &shadow = spotLight->GetShadowSettings();
            backend.SetSpotShadow(mSpotShadowDepthTexture, spotVP, shadow.mBiasConstant, shadow.mBiasSlope);
        }

        if (ctx.__targetWidth__ > 0u && ctx.__targetHeight__ > 0u) // 恢复视口为屏幕分辨率(准备正常渲染)
            glViewport(0, 0, static_cast<GLsizei>(ctx.__targetWidth__), static_cast<GLsizei>(ctx.__targetHeight__));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}