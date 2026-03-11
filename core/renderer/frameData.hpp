#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstdint>

namespace core
{
    constexpr uint32_t MaxDirectionalLights = 4u;
    constexpr uint32_t MaxPointLights = 16u;
    constexpr uint32_t MaxSpotLights = 8u;

    struct DirectionalLightGPU final
    {
        alignas(16) glm::vec4 direction{0.f, -1.f, 0.f, 0.f};
        alignas(16) glm::vec4 colorIntensity{1.f, 1.f, 1.f, 1.f}; // xyz为颜色, w为强度
    };

    struct PointLightGPU final
    {
        alignas(16) glm::vec4 positionRange;  // xyz为位置, w为为范围
        alignas(16) glm::vec4 colorIntensity; // rgb为颜色, a为强度
        alignas(16) glm::vec4 attenuation;    // x为kc, y为k1, z为k2, w保留
    };

    struct SpotLightGPU final
    {
        alignas(16) glm::vec4 positionRange;       // xyz为位置, w为存储范围
        alignas(16) glm::vec4 directionInnerCos;   // xyz为方向, w为innerCos
        alignas(16) glm::vec4 attenuationOuterCos; // xyz为衰减, w为outerCos
        alignas(16) glm::vec4 colorIntensity;      // rgb为颜色, a为强度
    };

    /*
        每帧只更新一次, 所有物体共享的数据使用UBO传输
        每个物体都不同, 每帧需要更新多次直接使用Uniform
    */

    struct FrameBlockData final // UBO-0
    {
        alignas(16) glm::mat4 uV{1.f};                          // 视图矩阵
        alignas(16) glm::mat4 uP{1.f};                          // 投影矩阵
        alignas(16) glm::vec4 uViewPosTime{0.f, 0.f, 0.f, 0.f}; // 视点位置(w为时间)
    };

    struct LightBlockData final // UBO-1
    {
        alignas(16) glm::ivec4 uLightMeta{0, 0, 0, 0}; // x为定向光数量, y为点光数量, z为聚光数量, w保留
        alignas(16) std::array<DirectionalLightGPU, MaxDirectionalLights> uDirectionalLights{};
        alignas(16) std::array<PointLightGPU, MaxPointLights> uPointLights{};
        alignas(16) std::array<SpotLightGPU, MaxSpotLights> uSpotLights{};
    };
}