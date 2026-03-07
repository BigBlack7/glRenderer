#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstdint>

namespace core
{
    constexpr uint32_t MaxDirectionalLights = 4u;

    struct DirectionalLightGpu final
    {
        alignas(16) glm::vec4 direction{0.f, -1.f, 0.f, 0.f};
        alignas(16) glm::vec4 colorIntensity{1.f, 1.f, 1.f, 1.f}; // w分量存储强度
    };

    struct FrameBlockData final // UBO-0
    {
        alignas(16) glm::mat4 uV{1.f};                          // 视图矩阵
        alignas(16) glm::mat4 uP{1.f};                          // 投影矩阵
        alignas(16) glm::vec4 uViewPosTime{0.f, 0.f, 0.f, 0.f}; // 视点位置(w分量存储时间)
    };

    struct LightBlockData final // UBO-1
    {
        alignas(16) glm::ivec4 uDirectionalLightMeta{0, 0, 0, 0}; // x分量存储定向光数量, yzw分量保留
        alignas(16) std::array<DirectionalLightGpu, MaxDirectionalLights> uDirectionalLights{};
    };
}