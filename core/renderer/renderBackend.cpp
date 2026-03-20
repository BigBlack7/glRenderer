#include "renderBackend.hpp"
#include "camera/camera.hpp"
#include "geometry/mesh.hpp"
#include "light/light.hpp"
#include "material/material.hpp"
#include "texture/environmentMap.hpp"
#include "shader/shader.hpp"
#include "texture/texture.hpp"
#include "utils/logger.hpp"
#include <limits>
#include <cstdint>

namespace core
{
    namespace detail
    {
        GLenum ToGLCompareOp(CompareOp op)
        {
            switch (op)
            {
            case CompareOp::Never:
                return GL_NEVER;
            case CompareOp::Less:
                return GL_LESS;
            case CompareOp::Equal:
                return GL_EQUAL;
            case CompareOp::LessEqual:
                return GL_LEQUAL;
            case CompareOp::Greater:
                return GL_GREATER;
            case CompareOp::NotEqual:
                return GL_NOTEQUAL;
            case CompareOp::GreaterEqual:
                return GL_GEQUAL;
            case CompareOp::Always:
                return GL_ALWAYS;
            }
            return GL_LESS;
        }

        GLenum ToGLStencilOp(StencilOp op)
        {
            switch (op)
            {
            case StencilOp::Keep:
                return GL_KEEP;
            case StencilOp::Zero:
                return GL_ZERO;
            case StencilOp::Replace:
                return GL_REPLACE;
            case StencilOp::IncrClamp:
                return GL_INCR;
            case StencilOp::DecrClamp:
                return GL_DECR;
            case StencilOp::Invert:
                return GL_INVERT;
            case StencilOp::IncrWrap:
                return GL_INCR_WRAP;
            case StencilOp::DecrWrap:
                return GL_DECR_WRAP;
            }
            return GL_KEEP;
        }

        GLenum ToGLBlendFactor(BlendFactor f)
        {
            switch (f)
            {
            case BlendFactor::Zero:
                return GL_ZERO;
            case BlendFactor::One:
                return GL_ONE;
            case BlendFactor::SrcColor:
                return GL_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor:
                return GL_ONE_MINUS_SRC_COLOR;
            case BlendFactor::DstColor:
                return GL_DST_COLOR;
            case BlendFactor::OneMinusDstColor:
                return GL_ONE_MINUS_DST_COLOR;
            case BlendFactor::SrcAlpha:
                return GL_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha:
                return GL_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstAlpha:
                return GL_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha:
                return GL_ONE_MINUS_DST_ALPHA;
            case BlendFactor::ConstantColor:
                return GL_CONSTANT_COLOR;
            case BlendFactor::OneMinusConstantColor:
                return GL_ONE_MINUS_CONSTANT_COLOR;
            case BlendFactor::ConstantAlpha:
                return GL_CONSTANT_ALPHA;
            case BlendFactor::OneMinusConstantAlpha:
                return GL_ONE_MINUS_CONSTANT_ALPHA;
            }
            return GL_ONE;
        }

        GLenum ToGLBlendOp(BlendOp op)
        {
            switch (op)
            {
            case BlendOp::Add:
                return GL_FUNC_ADD;
            case BlendOp::Subtract:
                return GL_FUNC_SUBTRACT;
            case BlendOp::ReverseSubtract:
                return GL_FUNC_REVERSE_SUBTRACT;
            case BlendOp::Min:
                return GL_MIN;
            case BlendOp::Max:
                return GL_MAX;
            }
            return GL_FUNC_ADD;
        }

        GLenum ToGLCullMode(CullMode mode)
        {
            switch (mode)
            {
            case CullMode::Front:
                return GL_FRONT;
            case CullMode::Back:
                return GL_BACK;
            case CullMode::FrontAndBack:
                return GL_FRONT_AND_BACK;
            case CullMode::None:
                return GL_BACK;
            }
            return GL_BACK;
        }

        GLenum ToGLFrontFace(FrontFace face)
        {
            return (face == FrontFace::CW) ? GL_CW : GL_CCW;
        }

        GLenum ToGLFillMode(FillMode mode)
        {
            switch (mode)
            {
            case FillMode::Fill:
                return GL_FILL;
            case FillMode::Line:
                return GL_LINE;
            case FillMode::Point:
                return GL_POINT;
            }
            return GL_FILL;
        }

        GLenum ToGLPolygonOffsetMode(PolygonOffsetMode mode)
        {
            switch (mode)
            {
            case PolygonOffsetMode::Fill:
                return GL_POLYGON_OFFSET_FILL;
            case PolygonOffsetMode::Line:
                return GL_POLYGON_OFFSET_LINE;
            case PolygonOffsetMode::Point:
                return GL_POLYGON_OFFSET_POINT;
            }
            return GL_POLYGON_OFFSET_FILL;
        }
    }

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

        // 创建全屏三角形VAO, 用于绘制全屏纹理
        if (mFullscreenTriangleVAO == 0)
            glGenVertexArrays(1, &mFullscreenTriangleVAO);

        if (mFullscreenTriangleVAO == 0) // 全屏三角形VAO创建失败
        {
            GL_CRITICAL("[RenderBackend] Init Failed: Fullscreen Triangle VAO Creation Failed");
            mFrameBlockUBO = UniformBuffer{};
            mLightBlockUBO = UniformBuffer{};
            mInitialized = false;
            return false;
        }

        // 初始化状态缓存和光照缓存
        mStateCache.Reset();
        mCachedDirectionalCount = std::numeric_limits<uint32_t>::max();
        mCachedDirectionalVersions.fill(std::numeric_limits<uint64_t>::max());
        mCachedPointCount = std::numeric_limits<uint32_t>::max();
        mCachedPointVersions.fill(std::numeric_limits<uint64_t>::max());
        mCachedSpotCount = std::numeric_limits<uint32_t>::max();
        mCachedSpotVersions.fill(std::numeric_limits<uint64_t>::max());
        mProgramBlockBoundCache.clear();
        mMaterialBindingCache.clear();

        mInstancedLayoutCache.clear();
        mInstanceModelScratch.clear();
        mInstanceNormalScratch.clear();

        // 初始化默认渲染状态 - 不透明状态
        mHasAppliedState = false;
        ApplyRenderState(MakeOpaqueState());

        // 启用立方体贴图无缝采样
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        mInitialized = true;
        return true;
    }

    void RenderBackend::Shutdown()
    {
        mFrameBlockUBO = UniformBuffer{};
        mLightBlockUBO = UniformBuffer{};
        mInstanceModelBuffer = InstanceBuffer{};
        mInstanceNormalBuffer = InstanceBuffer{};
        mStateCache.Reset();
        mProgramBlockBoundCache.clear();
        mMaterialBindingCache.clear();
        mInstancedLayoutCache.clear();
        mInstanceModelScratch.clear();
        mInstanceNormalScratch.clear();
        mInitialized = false;
        mHasAppliedState = false;

        if (mFullscreenTriangleVAO != 0)
        {
            glDeleteVertexArrays(1, &mFullscreenTriangleVAO);
            mFullscreenTriangleVAO = 0;
        }
    }

    void RenderBackend::BeginRenderTarget(const FrameBuffer *target, bool clearColor, bool clearDepth, bool clearStencil)
    {
        if (target && target->IsValid())
            target->Bind(); // 绑定自定义帧缓冲
        else
            FrameBuffer::BindDefault(); // 绑定默认帧缓冲

        if (!clearColor && !clearDepth && !clearStencil) // 如果不需要清除任何缓冲区直接返回
            return;

        /*
            不管原状态直接显式设置为清除所需的状态
            确保清除操作一定能成功, 避免状态污染
        */
        glDisable(GL_SCISSOR_TEST);                      // 禁用裁剪测试
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // 启用所有颜色通道写入
        glDepthMask(GL_TRUE);                            // 启用深度写入
        glStencilMaskSeparate(GL_FRONT, 0xFF);           // 启用正面模板写入
        glStencilMaskSeparate(GL_BACK, 0xFF);            // 启用背面模板写入

        if (clearColor)
        {
            const GLfloat color[4] = {mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a};
            glClearBufferfv(GL_COLOR, 0, color);
        }

        if (clearDepth && clearStencil)
        {
            glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.f, 0);
        }
        else
        {
            if (clearDepth)
            {
                const GLfloat depth = 1.f;
                glClearBufferfv(GL_DEPTH, 0, &depth);
            }

            if (clearStencil)
            {
                const GLint stencil = 0;
                glClearBufferiv(GL_STENCIL, 0, &stencil);
            }
        }
        mHasAppliedState = false;
    }

    void RenderBackend::EndRenderTarget() {}

    void RenderBackend::BindProgramBlocks(const Shader &shader)
    {
        // 使用缓存检查避免重复绑定
        if (mProgramBlockBoundCache.find(shader.GetID()) != mProgramBlockBoundCache.end())
            return;

        // 绑定帧数据块到槽位0, 光照数据块到槽位1
        shader.BindUniformBlock("FrameBlock", FrameBlockBindingPoint, true);
        shader.BindUniformBlock("LightBlock", LightBlockBindingPoint, false);
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
        const auto &bindings = GetMaterialBindings(shader);

        for (size_t i = 0; i < textures.size(); ++i)
        {
            const auto &tex = textures[i];
            const GLint samplerLoc = bindings.mSampler[i];
            if (!tex || samplerLoc < 0)
                continue;

            const uint32_t unit = static_cast<uint32_t>(i);
            if (mStateCache.BindTexture2D(unit, tex->GetID()))
                ++stats.__textureBinds__;

            shader.SetInt(samplerLoc, static_cast<int>(unit));
        }

        if (bindings.mMaterialFlags >= 0)
            shader.SetUInt(bindings.mMaterialFlags, material.GetFeatureFlags());

        const RenderStateDesc &state = material.GetRenderState();
        if (bindings.mAlphaMode >= 0)
            shader.SetUInt(bindings.mAlphaMode, static_cast<uint32_t>(state.mDomain));
        if (bindings.mAlphaCutoff >= 0)
            shader.SetFloat(bindings.mAlphaCutoff, state.mAlphaCutoff);
        if (bindings.mOpacity >= 0)
            shader.SetFloat(bindings.mOpacity, state.mOpacity);

        if (bindings.mBaseColor >= 0)
            shader.SetVec3(bindings.mBaseColor, material.GetBaseColor());
        if (bindings.mShininess >= 0)
            shader.SetFloat(bindings.mShininess, material.GetShininess());

        for (const auto &[name, value] : material.GetFloatParams())
            shader.SetFloatOptional(name, value);
        for (const auto &[name, value] : material.GetIntParams())
            shader.SetIntOptional(name, value);
        for (const auto &[name, value] : material.GetUIntParams())
            shader.SetUIntOptional(name, value);
        for (const auto &[name, value] : material.GetVec3Params())
            shader.SetVec3Optional(name, value);
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

    void RenderBackend::ApplyRenderState(const RenderStateDesc &state)
    {
        const bool hasPrev = mHasAppliedState;       // 是否之前有应用过状态
        const RenderStateDesc &prev = mAppliedState; // 获取之前的状态

        /* 统一只有状态变化时才应用 */
        // Depth
        if (!hasPrev || prev.mDepth.mDepthTest != state.mDepth.mDepthTest)
        {
            if (state.mDepth.mDepthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
        }

        // 深度函数和写入掩码
        if (!hasPrev || prev.mDepth.mDepthFunc != state.mDepth.mDepthFunc)
            glDepthFunc(detail::ToGLCompareOp(state.mDepth.mDepthFunc));

        if (!hasPrev || prev.mDepth.mDepthWrite != state.mDepth.mDepthWrite)
            glDepthMask(state.mDepth.mDepthWrite ? GL_TRUE : GL_FALSE);

        // Blend
        if (!hasPrev || prev.mBlend.mBlend != state.mBlend.mBlend)
        {
            if (state.mBlend.mBlend)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
        }

        // 混合参数变化检测
        const bool blendParamChanged =
            !hasPrev ||
            !prev.mBlend.mBlend ||
            prev.mBlend.mSrcColor != state.mBlend.mSrcColor ||
            prev.mBlend.mDstColor != state.mBlend.mDstColor ||
            prev.mBlend.mSrcAlpha != state.mBlend.mSrcAlpha ||
            prev.mBlend.mDstAlpha != state.mBlend.mDstAlpha ||
            prev.mBlend.mColorOp != state.mBlend.mColorOp ||
            prev.mBlend.mAlphaOp != state.mBlend.mAlphaOp;

        // 只有混合开启且参数变化时才更新混合参数
        if (state.mBlend.mBlend && blendParamChanged)
        {
            glBlendFuncSeparate(
                detail::ToGLBlendFactor(state.mBlend.mSrcColor),
                detail::ToGLBlendFactor(state.mBlend.mDstColor),
                detail::ToGLBlendFactor(state.mBlend.mSrcAlpha),
                detail::ToGLBlendFactor(state.mBlend.mDstAlpha));
            glBlendEquationSeparate(
                detail::ToGLBlendOp(state.mBlend.mColorOp),
                detail::ToGLBlendOp(state.mBlend.mAlphaOp));
        }

        // Cull
        const bool cullEnabledNow = state.mRaster.mFaceCull && state.mRaster.mCullFace != CullMode::None;
        const bool cullEnabledPrev = hasPrev && prev.mRaster.mFaceCull && prev.mRaster.mCullFace != CullMode::None;

        // 只有剔除开关状态变化时才调用
        if (!hasPrev || cullEnabledPrev != cullEnabledNow)
        {
            if (cullEnabledNow)
                glEnable(GL_CULL_FACE);
            else
                glDisable(GL_CULL_FACE);
        }

        // 剔除参数只有启用且变化时才更新
        if (cullEnabledNow && (!hasPrev || !cullEnabledPrev ||
                               prev.mRaster.mFrontFace != state.mRaster.mFrontFace ||
                               prev.mRaster.mCullFace != state.mRaster.mCullFace))
        {
            glFrontFace(detail::ToGLFrontFace(state.mRaster.mFrontFace));
            glCullFace(detail::ToGLCullMode(state.mRaster.mCullFace));
        }

        // Polygon mode
        if (!hasPrev || prev.mRaster.mFillMode != state.mRaster.mFillMode)
            glPolygonMode(GL_FRONT_AND_BACK, detail::ToGLFillMode(state.mRaster.mFillMode));

        // Scissor
        if (!hasPrev || prev.mRaster.mScissorTest != state.mRaster.mScissorTest)
        {
            if (state.mRaster.mScissorTest)
                glEnable(GL_SCISSOR_TEST);
            else
                glDisable(GL_SCISSOR_TEST);
        }

        // MSAA
        if (!hasPrev || prev.mRaster.mMultiSample != state.mRaster.mMultiSample)
        {
            if (state.mRaster.mMultiSample)
                glEnable(GL_MULTISAMPLE);
            else
                glDisable(GL_MULTISAMPLE);
        }

        // Color mask
        if (!hasPrev ||
            prev.mRaster.mColorWriteR != state.mRaster.mColorWriteR ||
            prev.mRaster.mColorWriteG != state.mRaster.mColorWriteG ||
            prev.mRaster.mColorWriteB != state.mRaster.mColorWriteB ||
            prev.mRaster.mColorWriteA != state.mRaster.mColorWriteA)
        {
            glColorMask(
                state.mRaster.mColorWriteR ? GL_TRUE : GL_FALSE,
                state.mRaster.mColorWriteG ? GL_TRUE : GL_FALSE,
                state.mRaster.mColorWriteB ? GL_TRUE : GL_FALSE,
                state.mRaster.mColorWriteA ? GL_TRUE : GL_FALSE);
        }

        // Polygon offset
        const bool polyOffsetChanged =
            !hasPrev ||
            prev.mPolygonOffset.mPolygonOffset != state.mPolygonOffset.mPolygonOffset ||
            prev.mPolygonOffset.mPolygonOffsetType != state.mPolygonOffset.mPolygonOffsetType ||
            prev.mPolygonOffset.mFactor != state.mPolygonOffset.mFactor ||
            prev.mPolygonOffset.mUnit != state.mPolygonOffset.mUnit;

        if (polyOffsetChanged)
        {
            // 先全部禁用, 再按需启用
            glDisable(GL_POLYGON_OFFSET_FILL);
            glDisable(GL_POLYGON_OFFSET_LINE);
            glDisable(GL_POLYGON_OFFSET_POINT);

            if (state.mPolygonOffset.mPolygonOffset)
            {
                glEnable(detail::ToGLPolygonOffsetMode(state.mPolygonOffset.mPolygonOffsetType));
                glPolygonOffset(state.mPolygonOffset.mFactor, state.mPolygonOffset.mUnit);
            }
        }

        // Stencil
        if (!hasPrev || prev.mStencil.mStencilTest != state.mStencil.mStencilTest)
        {
            if (state.mStencil.mStencilTest)
                glEnable(GL_STENCIL_TEST);
            else
                glDisable(GL_STENCIL_TEST);
        }

        if (state.mStencil.mStencilTest)
        {
            // 检测两个模板面状态是否不同
            auto FaceChanged = [](const StencilFaceState &a, const StencilFaceState &b) -> bool
            {
                return a.mStencilFunc != b.mStencilFunc ||
                       a.mReference != b.mReference ||
                       a.mStencilFuncMask != b.mStencilFuncMask ||
                       a.mStencilMask != b.mStencilMask ||
                       a.mSfail != b.mSfail ||
                       a.mZfail != b.mZfail ||
                       a.mZPass != b.mZPass;
            };

            // 综合检测模板状态是否变化
            const bool stencilChanged =
                !hasPrev ||
                !prev.mStencil.mStencilTest ||
                prev.mStencil.mSeparateFace != state.mStencil.mSeparateFace ||
                FaceChanged(prev.mStencil.mFront, state.mStencil.mFront) ||
                FaceChanged(prev.mStencil.mBack, state.mStencil.mBack);

            if (stencilChanged)
            {
                const auto &front = state.mStencil.mFront;
                const auto &back = state.mStencil.mBack;

                // 根据是否双面不同设置分别处理
                if (state.mStencil.mSeparateFace)
                {
                    glStencilFuncSeparate(GL_FRONT, detail::ToGLCompareOp(front.mStencilFunc), static_cast<GLint>(front.mReference), front.mStencilFuncMask);
                    glStencilMaskSeparate(GL_FRONT, front.mStencilMask);
                    glStencilOpSeparate(GL_FRONT, detail::ToGLStencilOp(front.mSfail), detail::ToGLStencilOp(front.mZfail), detail::ToGLStencilOp(front.mZPass));

                    glStencilFuncSeparate(GL_BACK, detail::ToGLCompareOp(back.mStencilFunc), static_cast<GLint>(back.mReference), back.mStencilFuncMask);
                    glStencilMaskSeparate(GL_BACK, back.mStencilMask);
                    glStencilOpSeparate(GL_BACK, detail::ToGLStencilOp(back.mSfail), detail::ToGLStencilOp(back.mZfail), detail::ToGLStencilOp(back.mZPass));
                }
                else
                {
                    glStencilFunc(detail::ToGLCompareOp(front.mStencilFunc), static_cast<GLint>(front.mReference), front.mStencilFuncMask);
                    glStencilMask(front.mStencilMask);
                    glStencilOp(detail::ToGLStencilOp(front.mSfail), detail::ToGLStencilOp(front.mZfail), detail::ToGLStencilOp(front.mZPass));
                }
            }
        }

        mAppliedState = state;
        mHasAppliedState = true;
    }

    void RenderBackend::DrawItems(std::span<const DrawItem> items, RenderProfiler &stats)
    {
        const Material *lastMaterial = nullptr;
        uint64_t lastMaterialVersion = 0u;
        const Shader *lastShader = nullptr;

        for (const DrawItem &item : items)
        {
            if (!item.__shader__ || !item.__material__ || !item.__mesh__)
                continue;

            const Shader &shader = *item.__shader__;
            const Material &material = *item.__material__;

            if (mStateCache.UseProgram(shader.GetID()))
            {
                ++stats.__programBinds__;
                BindProgramBlocks(shader);
                lastShader = &shader;
                lastMaterial = nullptr;
                lastMaterialVersion = 0u;
            }
            else if (lastShader != &shader)
            {
                BindProgramBlocks(shader);
                lastShader = &shader;
                lastMaterial = nullptr;
                lastMaterialVersion = 0u;
            }

            if (lastMaterial != &material || lastMaterialVersion != material.GetVersion())
            {
                ApplyRenderState(material.GetRenderState());
                ApplyMaterial(material, shader, stats);
                shader.SetUIntOptional("uUseInstancing", 0u);
                lastMaterial = &material;
                lastMaterialVersion = material.GetVersion();
            }

            shader.SetMat4Optional("uM", item.__world__);
            shader.SetMat3Optional("uN", item.__normal__);
            DrawMesh(*item.__mesh__, stats);
        }
    }

    void RenderBackend::EnsureInstancedLayout(const Mesh &mesh)
    {
        const GLuint vao = mesh.GetVAO();
        if (vao == 0 || !mInstanceModelBuffer.IsValid() || !mInstanceNormalBuffer.IsValid()) // 如果VAO无效或实例化缓冲区未初始化直接返回
            return;

        if (mInstancedLayoutCache.find(vao) != mInstancedLayoutCache.end()) // 如果该VAO的实例化布局已经设置过跳过重复设置
            return;

        mStateCache.BindVertexArray(vao); // 绑定当前网格的VAO到OpenGL状态

        // 设置模型矩阵属性
        mInstanceModelBuffer.Bind();
        constexpr GLsizei modelStride = static_cast<GLsizei>(sizeof(glm::mat4)); // 单个模型矩阵的字节跨度(64字节)
        for (uint32_t col = 0u; col < 4u; ++col)                                 // 遍历矩阵的4个列向量
        {
            const GLuint location = 3u + col;                                         // 属性位置从3开始(0-2是位置/法线/UV)
            const uintptr_t offset = static_cast<uintptr_t>(sizeof(glm::vec4) * col); // 当前列的偏移量

            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, modelStride, reinterpret_cast<const void *>(offset));
            glVertexAttribDivisor(location, 1u);
        }

        // 设置法线矩阵属性
        mInstanceNormalBuffer.Bind();
        constexpr GLsizei normalStride = static_cast<GLsizei>(sizeof(glm::vec4) * 3u); // 法线矩阵的字节跨度(48字节, 实际是3x3矩阵)
        for (uint32_t col = 0u; col < 3u; ++col)                                       // 遍历法线矩阵的3个列向量
        {
            const GLuint location = 7u + col; // 属性位置从7开始(接在模型矩阵之后)
            const uintptr_t offset = static_cast<uintptr_t>(sizeof(glm::vec4) * col);

            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, normalStride, reinterpret_cast<const void *>(offset));
            glVertexAttribDivisor(location, 1u);
        }

        InstanceBuffer::Unbind();
        mInstancedLayoutCache.insert(vao); // 将该VAO标记为已设置实例化布局
    }

    bool RenderBackend::UploadInstanceData(std::span<const DrawItem> items)
    {
        if (items.empty()) // 如果没有绘制项直接返回失败
            return false;

        // 准备模型矩阵和法线矩阵数据
        mInstanceModelScratch.clear();
        mInstanceNormalScratch.clear();
        mInstanceModelScratch.reserve(items.size());
        mInstanceNormalScratch.reserve(items.size() * 3u);

        for (const DrawItem &item : items)
        {
            mInstanceModelScratch.push_back(item.__world__); // 收集世界变换矩阵

            // 收集法线矩阵, 0填充按16字节对齐
            const glm::mat3 &n = item.__normal__;
            mInstanceNormalScratch.emplace_back(n[0], 0.f);
            mInstanceNormalScratch.emplace_back(n[1], 0.f);
            mInstanceNormalScratch.emplace_back(n[2], 0.f);
        }

        // 上传模型矩阵数据
        const bool modelOk = mInstanceModelBuffer.Upload(mInstanceModelScratch.data(), mInstanceModelScratch.size() * sizeof(glm::mat4));
        // 上传法线矩阵数据
        const bool normalOk = mInstanceNormalBuffer.Upload(mInstanceNormalScratch.data(), mInstanceNormalScratch.size() * sizeof(glm::vec4));

        return modelOk && normalOk;
    }

    void RenderBackend::DrawMeshInstanced(const Mesh &mesh, uint32_t instanceCount, RenderProfiler &stats)
    {
        if (instanceCount == 0u || mesh.GetVAO() == 0u || mesh.GetVertexCount() == 0u) // 无效参数检查
            return;

        mStateCache.BindVertexArray(mesh.GetVAO());

        if (mesh.GetIndexCount() > 0u) // 如果网格有索引数据使用索引实例化渲染
        {
            glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(mesh.GetIndexCount()), GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(instanceCount));
            stats.__triangles__ += (mesh.GetIndexCount() / 3u) * instanceCount;
        }
        else // 如果网格没有索引数据使用顶点实例化渲染
        {
            glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.GetVertexCount()), static_cast<GLsizei>(instanceCount));
            stats.__triangles__ += (mesh.GetVertexCount() / 3u) * instanceCount;
        }

        ++stats.__drawCalls__;
    }

    void RenderBackend::DrawInstancedItems(std::span<const DrawItem> items, RenderProfiler &stats)
    {
        if (items.empty()) // 没有绘制项直接返回
            return;

        const DrawItem &head = items.front();                         // 获取第一个绘制项作为参考
        if (!head.__shader__ || !head.__material__ || !head.__mesh__) // 如果着色器/材质/网格任一无效直接返回
            return;

        // 一致性检查确保所有绘制项使用相同的资源组合
        for (const DrawItem &item : items)
        {
            if (item.__shader__ != head.__shader__ ||
                item.__material__ != head.__material__ ||
                item.__mesh__ != head.__mesh__)
            {
                DrawItems(items, stats); // 发现不一致时, 退回到普通逐个绘制模式
                return;
            }
        }

        const Shader &shader = *head.__shader__;
        const Material &material = *head.__material__;

        if (mStateCache.UseProgram(shader.GetID()))
            ++stats.__programBinds__;

        BindProgramBlocks(shader);
        ApplyRenderState(material.GetRenderState());
        ApplyMaterial(material, shader, stats);

        shader.SetUIntOptional("uUseInstancing", 1u); // 通知着色器启用实例化渲染

        if (!UploadInstanceData(items)) // 上传实例化数据到GPU缓冲区
        {
            // 数据上传失败时, 退回到普通绘制模式
            shader.SetUIntOptional("uUseInstancing", 0u); // 关闭实例化标志
            DrawItems(items, stats);
            return;
        }

        EnsureInstancedLayout(*head.__mesh__);
        DrawMeshInstanced(*head.__mesh__, static_cast<uint32_t>(items.size()), stats);

        shader.SetUIntOptional("uUseInstancing", 0u); // 重置实例化标志避免影响后续绘制
    }

    const MaterialShaderBindings &RenderBackend::GetMaterialBindings(const Shader &shader)
    {
        const GLuint program = shader.GetID();
        if (const auto it = mMaterialBindingCache.find(program); it != mMaterialBindingCache.end())
            return it->second;

        return mMaterialBindingCache.emplace(program, BuildMaterialShaderBindings(shader)).first->second;
    }

    void RenderBackend::DrawOpaqueQueue(const RenderQueue &queue, RenderProfiler &stats)
    {
        const auto &items = queue.GetOpaqueItems();
        const auto &batches = queue.GetOpaqueBatches();

        if (items.empty())
            return;

        if (batches.empty()) // 没有批次信息退回到普通逐个绘制模式
        {
            DrawItems(items, stats);
            return;
        }

        constexpr uint32_t MinInstanceCount = 4u; // 最小实例化阈值: 只有≥4个相同物体才启用实例化

        for (const DrawBatch &batch : batches)
        {
            if (batch.__count__ == 0u) // 跳过空批次
                continue;

            const DrawItem *begin = items.data() + batch.__start__;       // 计算批次起始指针
            std::span<const DrawItem> batchItems(begin, batch.__count__); // 创建批次数据视图

            if (batch.__count__ >= MinInstanceCount)
                DrawInstancedItems(batchItems, stats); // 达到阈值使用实例化渲染
            else
                DrawItems(batchItems, stats);
        }
    }

    void RenderBackend::DrawTransparentQueue(const RenderQueue &queue, RenderProfiler &stats)
    {
        DrawItems(queue.GetTransparentItems(), stats);
    }

    void RenderBackend::ApplyPassState(const RenderStateDesc &state)
    {
        ApplyRenderState(state);
    }

    void RenderBackend::DrawFullscreenTexture(const Shader &shader, GLuint colorTexture, RenderProfiler &stats)
    {
        if (colorTexture == 0 || mFullscreenTriangleVAO == 0)
            return;

        if (mStateCache.UseProgram(shader.GetID()))
            ++stats.__programBinds__;

        if (mStateCache.BindTexture2D(0u, colorTexture))
            ++stats.__textureBinds__;

        shader.SetInt("uSceneSampler", 0); // 设置场景颜色采样器
        mStateCache.BindVertexArray(mFullscreenTriangleVAO);

        glDrawArrays(GL_TRIANGLES, 0, 3); // 绘制全屏三角形
        ++stats.__drawCalls__;
        ++stats.__triangles__;
    }

    void RenderBackend::DrawSkybox(const Shader &shader, const Mesh &cube, const EnvironmentMap &skybox, float intensity, RenderProfiler &stats)
    {
        if (!skybox.IsValid() || !cube.IsValid())
            return;

        if (mStateCache.UseProgram(shader.GetID()))
            ++stats.__programBinds__;

        BindProgramBlocks(shader);

        constexpr uint32_t cubeUnit = 0u;
        constexpr uint32_t panoUnit = 1u;

        shader.SetInt("uSkyboxCube", static_cast<int>(cubeUnit));
        shader.SetInt("uSkyboxPanorama", static_cast<int>(panoUnit));
        shader.SetInt("uSkyMode", skybox.IsCubeMap() ? 0 : 1);
        shader.SetFloat("uSkyboxIntensity", intensity);

        if (skybox.IsCubeMap())
        {
            if (mStateCache.BindTexture2D(cubeUnit, skybox.GetID()))
                ++stats.__textureBinds__;
            mStateCache.BindTexture2D(panoUnit, 0);
        }
        else
        {
            mStateCache.BindTexture2D(cubeUnit, 0);
            if (mStateCache.BindTexture2D(panoUnit, skybox.GetID()))
                ++stats.__textureBinds__;
        }

        DrawMesh(cube, stats);
    }
}