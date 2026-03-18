#include "application.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace core
{
    void Application::FramebufferSizeCallback(GLFWwindow *window, int width, int height)
    {
        Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (!app)
            return;

        app->mWidth = static_cast<uint32_t>(std::max(width, 1));
        app->mHeight = static_cast<uint32_t>(std::max(height, 1));

        if (app->mResizeCallback)
            app->mResizeCallback(app->mWidth, app->mHeight);
    }

    void Application::KeyBoardCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (app && app->mKeyCallback)
        {
            app->mKeyCallback(window, key, scancode, action, mods);
        }
    }

    void Application::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
    {
        Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (app && app->mMouseCallback)
        {
            app->mMouseCallback(window, button, action, mods);
        }
    }

    void Application::CursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
    {
        Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (app && app->mCursorCallback)
        {
            app->mCursorCallback(window, xpos, ypos);
        }
    }

    void Application::MouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
    {
        Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (app && app->mScrollCallback)
        {
            app->mScrollCallback(window, xoffset, yoffset);
        }
    }

    void Application::UpdateFPS(double currentTime)
    {
        ++mFrameCount;

        // 每秒更新一次标题, 平滑且开销小
        const double elapsed = currentTime - mLastFPSTime;
        if (elapsed >= 1.0)
        {
            mFPS = static_cast<double>(mFrameCount) / elapsed;
            mFrameCount = 0;
            mLastFPSTime = currentTime;

            std::string title = "GLRenderer    FPS: " + std::to_string(static_cast<int>(mFPS));
            glfwSetWindowTitle(mWindow.get(), title.c_str());
        }
    }

    Application &Application::GetApp()
    {
        // Meyers' Singleton, (cpp11)静态局部变量初始化是线程安全的
        static Application instance;
        return instance;
    }

    bool Application::Init(const uint32_t width, const uint32_t height)
    {
        mWidth = width;
        mHeight = height;
        // 初始化版本以及核心模式
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        mWindow.reset(glfwCreateWindow(mWidth, mHeight, "GLRenderer", nullptr, nullptr));
        if (!mWindow)
        {
            GL_CRITICAL("[Application] Failed To Create GLFW Window");
            return false;
        }

        // 获取主显示器的视频模式
        if (auto *primaryMonitor = glfwGetPrimaryMonitor())
        {
            if (const auto *vidMode = glfwGetVideoMode(primaryMonitor))
            {
                // 计算窗口居中位置
                int windowPosX = (vidMode->width - mWidth) / 2;
                int windowPosY = (vidMode->height - mHeight) / 2;

                // 设置窗口位置到屏幕中央
                glfwSetWindowPos(mWindow.get(), windowPosX, windowPosY);
                GL_INFO("[Application] Window centered at position ({}, {})", windowPosX, windowPosY);
            }
        }

        // 创建glfw上下文
        glfwMakeContextCurrent(mWindow.get());

        // 加载OpenGL函数指针
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            GL_CRITICAL("[Application] Failed To Initialize GLAD");
            return false;
        }

        // 将Application实例与窗口关联, 用于回调函数
        glfwSetWindowUserPointer(mWindow.get(), this);

        glfwSetFramebufferSizeCallback(mWindow.get(), FramebufferSizeCallback); // 窗口大小监听
        glfwSetKeyCallback(mWindow.get(), KeyBoardCallback);                    // 键盘按键监听
        glfwSetMouseButtonCallback(mWindow.get(), MouseButtonCallback);         // 鼠标点击监听
        glfwSetCursorPosCallback(mWindow.get(), CursorPositionCallback);        // 鼠标位置监听
        glfwSetScrollCallback(mWindow.get(), MouseScrollCallback);              // 鼠标滚轮监听

        // 时间初始化， 避免第一帧异常
        const double now = glfwGetTime();
        mLastFrameTime = now;
        mLastFPSTime = now;
        mDeltaTime = 0.0f;
        mFrameCount = 0;
        mFPS = 0.0;

        GL_INFO("[Application] GLWindow Init Success With {}x{}", mWidth, mHeight);
        return true;
    }

    bool Application::Update()
    {
        if (!mWindow)
            return false;

        const double now = glfwGetTime();
        mDeltaTime = static_cast<float>(now - mLastFrameTime);
        mLastFrameTime = now;

        // 防止拖动窗口/断点后dt过大
        if (mDeltaTime > 0.1f)
            mDeltaTime = 0.1f;

        // 处理窗口输入事件
        glfwPollEvents();
        // 更新FPS
        UpdateFPS(now);

        if (glfwWindowShouldClose(mWindow.get()))
            return false;

        // 交换缓冲区
        glfwSwapBuffers(mWindow.get());
        return true;
    }

    void Application::Destroy()
    {
        mWindow.reset();
        glfwTerminate();
        GL_INFO("[Application] GLWindow Shutdown");
    }
}