#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <memory>
#include <functional>

namespace core
{
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    using KeyBoardCallback = std::function<void(GLFWwindow *window, int key, int scancode, int action, int mods)>;

    class Application
    {
    private:
        uint32_t mWidth{0};
        uint32_t mHeight{0};
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> mWindow{nullptr, glfwDestroyWindow};

        ResizeCallback mResizeCallback{nullptr};
        KeyBoardCallback mKeyBoardCallback{nullptr};

        double mLastTime{0.0};
        uint32_t mFrameCount{0};
        double mFPS{0.0};

    private:
        Application() = default;
        ~Application() = default;

        // 禁用拷贝和赋值
        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;

        /// @brief 窗口大小监听回调函数
        /// @param window 窗体指针
        /// @param width 窗体宽度
        /// @param height 窗体高度
        static void FramebufferSizeCallback(GLFWwindow *window, int width, int height);

        /// @brief 键盘监听回调函数
        /// @param window 窗体指针
        /// @param key 字母按键值
        /// @param scancode 物理按键码
        /// @param action 动作, 按下为GLFW_PRESS, 释放为GLFW_RELEASE, 重复为GLFW_REPEAT
        /// @param mods 修饰键, 组合键为GLFW_MOD_SHIFT, GLFW_MOD_CONTROL, GLFW_MOD_ALT, GLFW_MOD_SUPER
        static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

        /// @brief 更新FPS统计
        void UpdateFPS();

    public:
        /// @brief 获取应用程序实例
        /// @return 应用程序实例指针
        static Application &GetInstance();

        /// @brief 初始化应用程序
        /// @param width 窗口宽度
        /// @param height 窗口高度
        /// @return 是否初始化成功
        bool Init(const uint32_t width = 1960, const uint32_t height = 1080);

        /// @brief 每帧更新应用程序
        /// @return 是否继续更新
        bool Update();

        /// @brief 销毁应用程序
        void Destroy();

        // getters
        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }

        // setters
        void SetResizeCallback(ResizeCallback callback) { mResizeCallback = std::move(callback); }
        void SetKeyBoardCallback(KeyBoardCallback callback) { mKeyBoardCallback = std::move(callback); }
    };
}