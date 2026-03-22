#pragma once
#include "renderer/frameContext.hpp"
#include <string_view>

namespace core
{
    class RenderBackend;

    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        /// @brief 获取渲染通道名称
        /// @return 通道名称字符串视图
        virtual std::string_view GetName() const noexcept = 0;

        /// @brief 执行渲染通道
        /// @param context 帧上下文, 包含当前帧的所有渲染数据
        /// @param backend 渲染后端, 提供OpenGL渲染功能
        virtual void Execute(FrameContext &context, RenderBackend &backend) = 0;
    };
}