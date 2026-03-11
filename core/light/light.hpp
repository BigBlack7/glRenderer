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

    /*
        点光源模型:
        I = I₀ / (kc + k1 * d + k2 * d²)
        其中I₀为光源强度, d为光源与被照点的距离, kc、k1、k2为衰减系数.
    */
    class PointLight final : public Light
    {
    private:
        glm::vec3 mPosition{0.f, 0.f, 0.f}; // 光源位置(世界空间)

        // 默认设置, 其它参数根据点光源的衰减模型进行调整
        float mRange{100.f};                          // 光源范围(超过此范围光照衰减为0)
        glm::vec3 mAttenuation{1.f, 0.045f, 0.0075f}; // 衰减系数(常数项kc、一次项k1、二次项k2)

    public:
        LightType GetType() const noexcept override { return LightType::Point; }

        void SetPosition(const glm::vec3 &position) noexcept
        {
            if (glm::length(position - mPosition) <= 1e-6f)
                return;

            mPosition = position;
            BumpVersion();
        }
        const glm::vec3 &GetPosition() const noexcept { return mPosition; }

        void SetRange(float range) noexcept
        {
            range = std::max(0.f, range);
            if (std::abs(mRange - range) <= 1e-6f)
                return;

            mRange = range;
            BumpVersion();
        }
        const float GetRange() const noexcept { return mRange; }

        void SetAttenuation(const glm::vec3 &attenuation) noexcept
        {
            glm::vec3 safe = glm::max(attenuation, glm::vec3(0.f));
            if (glm::length(safe - mAttenuation) <= 1e-6f)
                return;

            mAttenuation = safe;
            BumpVersion();
        }
        const glm::vec3 &GetAttenuation() const noexcept { return mAttenuation; }
    };

    /*
        聚光灯模型(光照衰减, 角度衰减):
        I = I₀ * clamp[(cos(θ) - cos(β))/(cos(α) - cos(β)), 0.f, 1.f] / (kc + k1 * d + k2 * d²)
        其中I₀为光源强度, d为光源与被照点的距离, kc、k1、k2为衰减系数, θ为被照点方向与聚光灯朝向的夹角.
    */
    class SpotLight final : public Light
    {
    private:
        glm::vec3 mPosition{0.f, 0.f, 0.f};                                // 光源位置(世界空间)
        glm::vec3 mDirection{glm::normalize(glm::vec3(-1.f, -1.f, -1.f))}; // 聚光灯朝向(世界空间)
        float mInnerCos{glm::cos(glm::radians(30.f))};                     // 聚光灯切光角α
        float mOuterCos{glm::cos(glm::radians(45.f))};                     // 聚光灯外切光角β

        // 默认设置, 其它参数根据聚光灯的衰减模型进行调整
        float mRange{32.f};                        // 光源范围(超过此范围光照衰减为0)
        glm::vec3 mAttenuation{1.f, 0.14f, 0.07f}; // 衰减系数(常数项kc、一次项k1、二次项k2)
    public:
        LightType GetType() const noexcept override { return LightType::Spot; }

        void SetPosition(const glm::vec3 &position) noexcept
        {
            if (glm::length(position - mPosition) <= 1e-6f)
                return;

            mPosition = position;
            BumpVersion();
        }

        const glm::vec3 &GetPosition() const noexcept { return mPosition; }

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

        void SetInnerAngle(float inner) noexcept
        {
            // [0°, 90°]范围
            inner = glm::clamp(inner, 0.f, 90.f);
            float innerCos = glm::cos(glm::radians(inner));
            if (std::abs(mInnerCos - innerCos) <= 1e-6f)
                return;

            mInnerCos = innerCos;
            BumpVersion();
        }

        float GetInnerCos() const noexcept { return mInnerCos; }

        void SetOuterAngle(float outer) noexcept
        {
            // [0°, 90°]范围, 且外切光角必须大于内切光角
            float inner = glm::degrees(glm::acos(mInnerCos));
            outer = glm::clamp(outer, inner + 10.f, 90.f);
            float outerCos = glm::cos(glm::radians(outer));
            if (std::abs(mOuterCos - outerCos) <= 1e-6f)
                return;

            mOuterCos = outerCos;
            BumpVersion();
        }
        float GetOuterCos() const noexcept { return mOuterCos; }

        void SetRange(float range) noexcept
        {
            range = std::max(0.f, range);
            if (std::abs(mRange - range) <= 1e-6f)
                return;

            mRange = range;
            BumpVersion();
        }
        const float GetRange() const noexcept { return mRange; }

        void SetAttenuation(const glm::vec3 &attenuation) noexcept
        {
            glm::vec3 safe = glm::max(attenuation, glm::vec3(0.f));
            if (glm::length(safe - mAttenuation) <= 1e-6f)
                return;

            mAttenuation = safe;
            BumpVersion();
        }
        const glm::vec3 &GetAttenuation() const noexcept { return mAttenuation; }
    };
}