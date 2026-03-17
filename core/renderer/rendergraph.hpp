#pragma once
#include "renderpass.hpp"
#include <memory>
#include <vector>

namespace core
{
    /*
     * - 支持动态添加渲染通道
     * - 按顺序执行所有通道
     * - 基本的编译时验证
     * 当前实现为线性图, 后续扩展DAG
     */
    class RenderGraph final
    {
    private:
        std::vector<std::unique_ptr<IRenderPass>> mPasses{};

    public:
        /// @brief 重置渲染图, 清空所有渲染通道
        void Reset();

        /// @brief 添加渲染通道到渲染图
        /// @param pass 要添加的渲染通道
        void AddPass(std::unique_ptr<IRenderPass> pass);

        /// @brief 编译渲染图
        /// @return 编译是否成功
        bool Compile() const;

        /// @brief 执行渲染图中的所有渲染通道
        /// @param context 帧上下文, 包含场景、摄像机等渲染数据
        /// @param backend 渲染后端, 提供OpenGL渲染功能
        void Execute(FrameContext &context, RenderBackend &backend);
    };
}