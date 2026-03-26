#include "shadowPass.hpp"
#include "camera/camera.hpp"
#include "light/light.hpp"
#include "renderer/renderBackend.hpp"
#include "shader/shader.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cfloat>

namespace core
{
    bool ShadowPass::EnsureInit()
    {
        if (mShadowShader && mShadowShader->GetID() != 0 && mShadowFBO != 0 && mShadowDepthTexture != 0)
            return true;

        if (mInitTried)
            return false;

        mInitTried = true;
        mShadowShader = std::make_shared<Shader>("pass/shadow.vert", "pass/shadow.frag");
        if (!mShadowShader || mShadowShader->GetID() == 0)
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

        if (mShadowFBO != 0)
        {
            glDeleteFramebuffers(1, &mShadowFBO);
            mShadowFBO = 0;
        }
    }

    bool ShadowPass::EnsureShadowResources()
    {
        if (mShadowFBO != 0 && mShadowDepthTexture != 0) // 检查是否已初始化
            return true;

        glGenFramebuffers(1, &mShadowFBO);      // 生成阴影帧缓冲区
        glGenTextures(1, &mShadowDepthTexture); // 生成阴影深度纹理

        if (mShadowFBO == 0 || mShadowDepthTexture == 0) // 检查是否生成成功
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

        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mShadowDepthTexture, 0); // 将深度纹理附加到FBO的深度附件点, 渲染到FBO时深度信息会自动写入这个纹理
        glDrawBuffer(GL_NONE);                                                                              // 禁用颜色绘制缓冲区
        glReadBuffer(GL_NONE);                                                                              // 禁用颜色读取缓冲区

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return status == GL_FRAMEBUFFER_COMPLETE;
    }

    // TODO 目前只支持单定向光光源阴影
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

    ShadowPass::~ShadowPass()
    {
        Release();
    }

    void ShadowPass::Execute(FrameContext &ctx, RenderBackend &backend)
    {
        ctx.__shadow__ = FrameContext::ShadowFrameData{}; // 初始化阴影帧数据
        backend.ClearDirectionalShadow();

        if (!ctx.__scene__ || !ctx.__camera__)
            return;

        const DirectionalLight *light = SelectDirectionalLight(ctx);
        if (!light)
            return;

        if (!EnsureInit())
            return;

        const glm::mat4 lightVP = BuildDirectionalLightVP(ctx, *light);

        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO);                                                      // 绑定阴影FBO(离屏渲染到阴影贴图)
        glViewport(0, 0, static_cast<GLsizei>(mShadowResolution), static_cast<GLsizei>(mShadowResolution)); // 设置视口为阴影贴图的分辨率

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
        glClear(GL_DEPTH_BUFFER_BIT);

        backend.DrawShadowDepth(
            ctx.__renderQueue__.GetOpaqueItems(),
            ctx.__renderQueue__.GetOpaqueBatches(),
            *mShadowShader,
            lightVP,
            ctx.__stats__);

        if (ctx.__targetWidth__ > 0u && ctx.__targetHeight__ > 0u) // 恢复视口为屏幕分辨率(准备正常渲染)
            glViewport(0, 0, static_cast<GLsizei>(ctx.__targetWidth__), static_cast<GLsizei>(ctx.__targetHeight__));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ctx.__shadow__.__hasDirectionalShadow__ = true;
        ctx.__shadow__.__directionalShadowTexture__ = mShadowDepthTexture;
        ctx.__shadow__.__directionalLightVP__ = lightVP;
        ctx.__shadow__.__directionalShadowResolution__ = mShadowResolution;

        backend.SetDirectionalShadow(mShadowDepthTexture, lightVP); // 设置方向阴影贴图和视图投影矩阵到渲染后端
    }
}