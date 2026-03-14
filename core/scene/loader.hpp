#pragma once
#include "material/material.hpp"
#include <filesystem>
#include <memory>
#include <unordered_map>

namespace core
{
    class Model;
    class Shader;

    struct ModelLoadOptions final
    {
        std::shared_ptr<Shader> __shader__{}; // 使用的着色器
        bool __flipTextureY__{true};          // 是否翻转纹理Y坐标
        bool __triangulate__{true};           // 是否三角化模型
        bool __genSmoothNormals__{true};      // 是否生成平滑法线

        using TextureOverrideMap = std::unordered_map<TextureSlot, std::filesystem::path>;
        TextureOverrideMap __globalTextureOverrides__{}; // 当模型文件未声明纹理时, 可手动注入全局纹理(作用于所有材质缺失槽位)
        bool __allowOverrideLoadedTextures__{false};     // 是否允许全局兜底纹理覆盖已自动解析的纹理
    };

    // TODO: 模型手动设置额外的纹理材质例如AO、Roughness等, 避免加载模型时解析不到这些纹理信息导致材质不完整
    class ModelLoader final
    {
    public:
        /// @brief 加载模型
        /// @param modelPath 模型路径
        /// @param options 加载选项
        /// @return 加载的模型
        static std::shared_ptr<Model> Load(const std::filesystem::path &modelPath, const ModelLoadOptions &options = {});
    };
}