#pragma once
#include <cstdint>

namespace core
{
    enum class MaterialDomain : uint8_t
    {
        Opaque = 0,     // 不透明材质
        Cutout = 1,     //  镂空材质
        Transparent = 2 // 透明度材质
    };

    enum class CompareOp : uint8_t // 深度比较操作
    {
        Never,        // 总是不通过
        Less,         // 小于
        Equal,        // 等于
        LessEqual,    // 小于等于
        Greater,      // 大于
        NotEqual,     // 不等于
        GreaterEqual, // 大于等于
        Always        // 总是通过
    };

    enum class StencilOp : uint8_t // 模板测试操作
    {
        Keep,      // 保持原值
        Zero,      // 清零
        Replace,   // 替换为参考值
        IncrClamp, // 递增并钳制
        DecrClamp, // 递减并钳制
        Invert,    // 按位取反
        IncrWrap,  // 递增并回绕
        DecrWrap   // 递减并回绕
    };

    enum class BlendFactor : uint8_t // 混合因子
    {
        Zero,                  // 0
        One,                   // 1
        SrcColor,              // 源颜色
        OneMinusSrcColor,      // 1 - 源颜色
        DstColor,              // 目标颜色
        OneMinusDstColor,      // 1 - 目标颜色
        SrcAlpha,              // 源Alpha
        OneMinusSrcAlpha,      // 1 - 源Alpha
        DstAlpha,              // 目标Alpha
        OneMinusDstAlpha,      // 1 - 目标Alpha
        ConstantColor,         // 常量颜色
        OneMinusConstantColor, // 1 - 常量颜色
        ConstantAlpha,         // 常量Alpha
        OneMinusConstantAlpha  // 1 - 常量Alpha
    };

    enum class BlendOp : uint8_t // 混合操作
    {
        Add,             // 加法
        Subtract,        // 减法
        ReverseSubtract, // 反向减法
        Min,             // 最小值
        Max,             // 最大值
    };

    enum class CullMode : uint8_t // 剔除模式
    {
        None,        // 不剔除
        Front,       // 剔除正面
        Back,        // 剔除背面
        FrontAndBack // 剔除正面和背面
    };

    enum class FrontFace : uint8_t // 正面方向
    {
        CCW, // 逆时针
        CW   // 顺时针
    };

    enum class FillMode : uint8_t // 填充模式
    {
        Fill, // 填充
        Line, // 线框
        Point // 点
    };

    enum class PolygonOffsetMode : uint8_t // 多边形偏移模式
    {
        Fill, // 填充
        Line, // 线框
        Point // 点
    };

    struct DepthState final // 深度状态
    {
        bool mDepthTest{true};                 // 是否开启深度测试
        CompareOp mDepthFunc{CompareOp::Less}; // 深度比较函数
        bool mDepthWrite{true};                // 是否写入深度缓冲

        bool operator==(const DepthState &) const = default;
    };

    struct PolygonOffsetState final // 多边形偏移状态
    {
        bool mPolygonOffset{false};                                    // 是否开启多边形偏移
        PolygonOffsetMode mPolygonOffsetType{PolygonOffsetMode::Fill}; // 多边形偏移模式
        float mFactor{1.f};                                            // 偏移因子
        float mUnit{1.f};                                              // 偏移单位

        bool operator==(const PolygonOffsetState &) const = default;
    };

    struct StencilFaceState final // 单面模板测试状态
    {
        CompareOp mStencilFunc{CompareOp::Always}; // 模板比较函数
        uint32_t mReference{1};                    // 参考值
        uint32_t mStencilFuncMask{0xFF};           // 比较掩码

        uint32_t mStencilMask{0xFF};          // 写入掩码
        StencilOp mSfail{StencilOp::Keep};    // 模板测试失败操作
        StencilOp mZfail{StencilOp::Keep};    // 深度测试失败操作
        StencilOp mZPass{StencilOp::Replace}; // 深度测试通过操作

        bool operator==(const StencilFaceState &) const = default;
    };

    struct StencilState final // 模板测试状态
    {
        bool mStencilTest{false};  // 是否开启模板测试
        bool mSeparateFace{false}; // 是否分离面
        StencilFaceState mFront{}; // 正面模板状态
        StencilFaceState mBack{};  // 反面模板状态

        bool operator==(const StencilState &) const = default;
    };

    struct BlendState final // 混合状态
    {
        bool mBlend{false}; // 是否开启混合

        BlendFactor mSrcColor{BlendFactor::SrcAlpha};         // 源颜色混合因子
        BlendFactor mDstColor{BlendFactor::OneMinusSrcAlpha}; // 目标颜色混合因子
        BlendOp mColorOp{BlendOp::Add};                       // 颜色混合操作

        BlendFactor mSrcAlpha{BlendFactor::One};              // 源Alpha混合因子
        BlendFactor mDstAlpha{BlendFactor::OneMinusSrcAlpha}; // 目标Alpha混合因子
        BlendOp mAlphaOp{BlendOp::Add};                       // Alpha混合操作

        bool operator==(const BlendState &) const = default;
    };

    struct RasterState final // 光栅化状态
    {
        bool mFaceCull{false};                // 是否开启面剔除
        FrontFace mFrontFace{FrontFace::CCW}; // 正面定义
        CullMode mCullFace{CullMode::Back};   // 剔除模式
        FillMode mFillMode{FillMode::Fill};   // 填充模式

        bool mScissorTest{false}; // 是否开启裁剪测试
        bool mMultiSample{true};  // 是否开启多重采样

        bool mColorWriteR{true}; // 是否写入红色通道
        bool mColorWriteG{true}; // 是否写入绿色通道
        bool mColorWriteB{true}; // 是否写入蓝色通道
        bool mColorWriteA{true}; // 是否写入Alpha通道

        bool operator==(const RasterState &) const = default;
    };

    struct RenderStateDesc final // 渲染状态描述
    {
        MaterialDomain mDomain{MaterialDomain::Opaque}; // 材质类型
        float mOpacity{1.f};                            // 材质透明度
        float mAlphaCutoff{0.5f};                       // Alpha测试阈值

        DepthState mDepth{}; // 深度状态
        PolygonOffsetState mPolygonOffset{}; // 多边形偏移状态
        StencilState mStencil{};             // 模板状态
        BlendState mBlend{};                 // 混合状态
        RasterState mRaster{};               // 光栅化状态

        bool operator==(const RenderStateDesc &) const = default;
    };

    inline RenderStateDesc MakeOpaqueState() // 不透明状态
    {
        RenderStateDesc s{};
        s.mDomain = MaterialDomain::Opaque;
        s.mDepth = DepthState{true, CompareOp::Less, true};
        s.mBlend.mBlend = false;
        s.mRaster.mFaceCull = false;
        s.mRaster.mCullFace = CullMode::Back;
        return s;
    }

    inline RenderStateDesc MakeCutoutState(float alphaCutoff = 0.5f) // 镂空状态
    {
        RenderStateDesc s = MakeOpaqueState();
        s.mDomain = MaterialDomain::Cutout;
        s.mAlphaCutoff = alphaCutoff;
        return s;
    }

    inline RenderStateDesc MakeTransparentState() // 透明状态
    {
        RenderStateDesc s = MakeOpaqueState();
        s.mDomain = MaterialDomain::Transparent;
        s.mDepth.mDepthWrite = false;
        s.mBlend.mBlend = true;
        s.mBlend.mSrcColor = BlendFactor::SrcAlpha;
        s.mBlend.mDstColor = BlendFactor::OneMinusSrcAlpha;
        s.mBlend.mColorOp = BlendOp::Add;
        s.mBlend.mSrcAlpha = BlendFactor::One;
        s.mBlend.mDstAlpha = BlendFactor::OneMinusSrcAlpha;
        s.mBlend.mAlphaOp = BlendOp::Add;
        return s;
    }
}