#pragma once
#include "light.hpp"
#include <optional>
#include <span>
#include <vector>
#include <unordered_map>
#include <typeindex>

namespace core
{
    using LightID = uint32_t;
    constexpr LightID InvalidLightID = std::numeric_limits<LightID>::max();

    class LightManager final
    {
    private:
        template <typename T>
        struct LightPool
        {
            std::vector<std::optional<T>> __lights__;
            std::vector<LightID> __freeIDs__;
            mutable std::vector<T> __uploadCache__;
            mutable bool __cacheDirty__{true};
        };

        std::unordered_map<std::type_index, void *> __lightPools__;

        template <typename T>
        LightPool<T> &GetPool()
        {
            auto typeIdx = std::type_index(typeid(T));
            if (__lightPools__.find(typeIdx) == __lightPools__.end())
            {
                __lightPools__[typeIdx] = new LightPool<T>();
            }
            return *static_cast<LightPool<T> *>(__lightPools__[typeIdx]);
        }

        template <typename T>
        const LightPool<T> &GetPool() const
        {
            auto typeIdx = std::type_index(typeid(T));
            auto it = __lightPools__.find(typeIdx);
            if (it == __lightPools__.end())
            {
                static LightPool<T> emptyPool;
                return emptyPool;
            }
            return *static_cast<const LightPool<T> *>(it->second);
        }

        template <typename T>
        [[nodiscard]] bool IsValidLightID(LightID id) const noexcept
        {
            const auto &pool = GetPool<T>();
            return id < static_cast<LightID>(pool.__lights__.size()) && pool.__lights__[id].has_value();
        }

        template <typename T>
        void RebuildUploadCache() const
        {
            auto &pool = GetPool<T>();
            pool.__uploadCache__.clear();
            pool.__uploadCache__.reserve(pool.__lights__.size());
            for (const auto &light : pool.__lights__)
            {
                if (light.has_value())
                {
                    pool.__uploadCache__.push_back(*light);
                }
            }
            pool.__cacheDirty__ = false;
        }

    public:
        LightManager() = default;
        ~LightManager()
        {
            for (auto &[typeIdx, poolPtr] : __lightPools__)
            {
                delete poolPtr;
            }
        }

        // 禁止复制
        LightManager(const LightManager &) = delete;
        LightManager &operator=(const LightManager &) = delete;

        // 允许移动
        LightManager(LightManager &&) noexcept = default;
        LightManager &operator=(LightManager &&) noexcept = default;

        template <typename T>
        [[nodiscard]] LightID CreateLight(const T &light = {})
        {
            auto &pool = GetPool<T>();
            if (!pool.__freeIDs__.empty())
            {
                const LightID id = pool.__freeIDs__.back();
                pool.__freeIDs__.pop_back();
                pool.__lights__[id] = light;
                pool.__cacheDirty__ = true;
                return id;
            }

            const LightID id = static_cast<LightID>(pool.__lights__.size());
            pool.__lights__.emplace_back(light);
            pool.__cacheDirty__ = true;
            return id;
        }

        template <typename T>
        bool RemoveLight(LightID id)
        {
            if (!IsValidLightID<T>(id))
                return false;

            auto &pool = GetPool<T>();
            pool.__lights__[id].reset();
            pool.__freeIDs__.push_back(id);
            pool.__cacheDirty__ = true;
            return true;
        }

        template <typename T>
        [[nodiscard]] T *GetLight(LightID id) noexcept
        {
            if (!IsValidLightID<T>(id))
                return nullptr;

            auto &pool = GetPool<T>();
            pool.__cacheDirty__ = true; // 返回可写指针，调用方可能修改灯光
            return &pool.__lights__[id].value();
        }

        template <typename T>
        [[nodiscard]] const T *GetLight(LightID id) const noexcept
        {
            if (!IsValidLightID<T>(id))
                return nullptr;
            return &GetPool<T>().__lights__[id].value();
        }

        template <typename T>
        [[nodiscard]] std::span<const T> GetLights() const noexcept
        {
            const auto &pool = GetPool<T>();
            if (pool.__cacheDirty__)
            {
                RebuildUploadCache<T>();
            }
            return pool.__uploadCache__;
        }

        template <typename T>
        void ClearLights()
        {
            auto &pool = GetPool<T>();
            pool.__lights__.clear();
            pool.__freeIDs__.clear();
            pool.__uploadCache__.clear();
            pool.__cacheDirty__ = true;
        }

        void ClearAllLights()
        {
            for (auto &[typeIdx, poolPtr] : __lightPools__)
            {
                delete poolPtr;
            }
            __lightPools__.clear();
        }
    };
}