#include "application.hpp"
#include <utils/logger.hpp>

namespace core
{
    void Application::FramebufferSizeCallback(GLFWwindow *window, int width, int height)
    {
        Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (app && app->mResizeCallback)
        {
            app->mResizeCallback(width, height);
        }
    }

    void Application::KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        if (app && app->mKeyBoardCallback)
        {
            app->mKeyBoardCallback(window, key, scancode, action, mods);
        }
    }

    void Application::UpdateFPS()
    {
        double currentTime = glfwGetTime();
        mFrameCount++;
        if (currentTime - mLastTime >= 2.0) // 每2秒更新一次FPS
        {
            mFPS = mFrameCount / (currentTime - mLastTime);
            mFrameCount = 0;
            mLastTime = currentTime;

            // 更新窗口标题
            std::string title = "GLRenderer    FPS: " + std::to_string(static_cast<int>(mFPS));
            glfwSetWindowTitle(mWindow.get(), title.c_str());
        }
    }

    Application &Application::GetInstance()
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
        if (mWindow == nullptr)
        {
            GL_ERROR("[Application] Failed To Create GLFW Window");
            return false;
        }
        // 创建glfw上下文
        glfwMakeContextCurrent(mWindow.get());

        // 加载OpenGL函数指针
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            GL_ERROR("[Application] Failed To Initialize GLAD");
            return false;
        }

        // 将Application实例与窗口关联, 用于回调函数
        glfwSetWindowUserPointer(mWindow.get(), this);

        glfwSetFramebufferSizeCallback(mWindow.get(), FramebufferSizeCallback); // 窗口大小监听
        glfwSetKeyCallback(mWindow.get(), KeyCallback);                         // 键盘监听

        GL_INFO("[Application] GLRenderer Init");
        return true;
    }

    bool Application::Update()
    {
        if (glfwWindowShouldClose(mWindow.get()))
        {
            mWindow.reset(); // 在终止GLFW前销毁窗口
            return false;
        }

        // FPS
        UpdateFPS();
        
        // 处理窗口输入事件
        glfwPollEvents();
        // 交换缓冲区
        glfwSwapBuffers(mWindow.get());
        return true;
    }

    void Application::Destroy()
    {
        glfwTerminate();
        GL_INFO("[Application] GLRenderer Shutdown");
    }
}