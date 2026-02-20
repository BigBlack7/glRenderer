#pragma once
#include <spdlog/spdlog.h>
#include <memory>

namespace core
{
    class Logger // 静态工具类, 日志管理
    {
    private:
        static std::shared_ptr<spdlog::logger> mLogger;
        static bool mInitialized;

        // 禁止实例化
        Logger() = delete;
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

    public:
        static void Init();
        static void Shutdown();
        static bool IsInitialized() { return mInitialized; }
        inline static std::shared_ptr<spdlog::logger> &GetLogger() { return mLogger; }
    };
}

// 日志宏
#define GL_TRACE(...) ::core::Logger::GetLogger()->trace(__VA_ARGS__)
#define GL_DEBUG(...) ::core::Logger::GetLogger()->debug(__VA_ARGS__)
#define GL_INFO(...) ::core::Logger::GetLogger()->info(__VA_ARGS__)
#define GL_WARN(...) ::core::Logger::GetLogger()->warn(__VA_ARGS__)
#define GL_ERROR(...) ::core::Logger::GetLogger()->error(__VA_ARGS__)
#define GL_CRITICAL(...) ::core::Logger::GetLogger()->critical(__VA_ARGS__)