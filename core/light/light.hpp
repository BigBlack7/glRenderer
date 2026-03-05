#pragma once
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <cstdint>

namespace core
{
    enum class LightType : uint8_t
    {
        Directional = 0,
        Point,
        Spot
    };

    class Light
    {
    private:
        bool mEnabled{true};
        glm::vec3 mColor{1.f, 1.f, 1.f};
        float mIntensity{1.f};
        uint64_t mVersion{0};

    protected:
        void BumpVersion() noexcept { ++mVersion; }

    public:
        virtual ~Light() = default;
        virtual LightType GetType() const noexcept = 0;

        void SetEnabled(bool enabled) noexcept
        {
            if (mEnabled == enabled)
                return;
            mEnabled = enabled;
            BumpVersion();
        }
        bool IsEnabled() const noexcept { return mEnabled; }

        void SetColor(const glm::vec3 &color) noexcept
        {
            if (glm::length(color - mColor) <= 1e-6f)
                return;
            mColor = color;
            BumpVersion();
        }
        const glm::vec3 &GetColor() const noexcept { return mColor; }

        void SetIntensity(float intensity) noexcept
        {
            intensity = std::max(0.f, intensity);
            if (std::abs(mIntensity - intensity) <= 1e-6f)
                return;

            mIntensity = intensity;
            BumpVersion();
        }
        float GetIntensity() const noexcept { return mIntensity; }

        uint64_t GetVersion() const noexcept { return mVersion; }
    };

    class DirectionalLight final : public Light
    {
    private:
        glm::vec3 mDirection{glm::normalize(glm::vec3(-1.f, -1.f, -1.f))}; // 光线传播方向(世界空间)

    public:
        LightType GetType() const noexcept override { return LightType::Directional; }

        void SetDirection(const glm::vec3 &direction) noexcept
        {
            if (glm::length(direction) <= 1e-6f)
                return;

            const glm::vec3 nd = glm::normalize(direction);
            if (glm::length(nd - mDirection) <= 1e-6f)
                return;

            mDirection = nd;
            BumpVersion();
        }

        const glm::vec3 &GetDirection() const noexcept { return mDirection; }
    };
}