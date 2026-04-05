#include "controller.hpp"
#include "application/application.hpp"
#include "utils/logger.hpp"
#include <imgui.h>
#include <cmath>

namespace core
{
    void CameraController::ToggleControl(GLFWwindow *window)
    {
        mControlEnabled = !mControlEnabled; // 切换控制模式: false->true 或 true->false
        mFirstMouse = true;                 // 每次切换后让下一帧鼠标事件做基准对齐, 防止刚控制时出现大幅跳转

        if (mControlEnabled)
        {
            // 进入控制模式: 隐藏并锁定鼠标到窗口
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) // 若平台支持 Raw Mouse, 则启用(更原始、更平滑)
            {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
            GL_INFO("[CameraController] Enter Roaming Mode");
        }
        else
        {
            // 退出控制模式: 恢复鼠标可见可自由移动
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            if (glfwRawMouseMotionSupported()) // 关闭 Raw Mouse
            {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
            GL_INFO("[CameraController] Exit Roaming Mode");
        }
    }

    void CameraController::UpdateCameraBasisFromYawPitch()
    {
        if (!mCamera)
            return;

        // 1) 用球坐标从 yaw/pitch 计算 front(前向)
        // yaw 控制水平旋转, pitch 控制上下抬头/低头
        glm::vec3 front;
        front.x = std::cos(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
        front.y = std::sin(glm::radians(mPitch));
        front.z = std::sin(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));

        // 2) 归一化, 确保方向向量长度为1
        front = glm::normalize(front);

        // 3) 右向量 = front × worldUp
        // 方向遵循右手系
        glm::vec3 right = glm::normalize(glm::cross(front, mWorldUp));

        // 4) 上向量 = right × front
        // 确保 up/right/front 三者正交
        glm::vec3 up = glm::normalize(glm::cross(right, front));

        mCamera->SetRight(right);
        mCamera->SetUp(up);
    }

    void CameraController::SyncYawPitchFromCamera()
    {
        if (!mCamera)
            return;

        glm::vec3 front = glm::normalize(glm::cross(mCamera->GetUp(), mCamera->GetRight()));
        mPitch = glm::degrees(std::asin(front.y));
        mYaw = glm::degrees(std::atan2(front.z, front.x));
    }

    void CameraController::SetCamera(const std::shared_ptr<Camera> &camera)
    {
        mCamera = camera;
        SyncYawPitchFromCamera(); // 让控制器角度与相机当前姿态同步
    }

    void CameraController::Update(float deltaTime)
    {
        // 仅在控制模式且已绑定相机时更新
        if (!mControlEnabled || !mCamera)
            return;

        const glm::vec3 forward = glm::normalize(glm::cross(mCamera->GetUp(), mCamera->GetRight()));
        const glm::vec3 right = glm::normalize(mCamera->GetRight());

        glm::vec3 move(0.f);
        if (IsKeyDown(GLFW_KEY_W)) // 前
            move += forward;
        if (IsKeyDown(GLFW_KEY_S)) // 后
            move -= forward;
        if (IsKeyDown(GLFW_KEY_D)) // 右
            move += right;
        if (IsKeyDown(GLFW_KEY_A)) // 左
            move -= right;

        // 上下沿世界Y轴移动
        if (IsKeyDown(GLFW_KEY_Q)) // 上
            move += mWorldUp;
        if (IsKeyDown(GLFW_KEY_E)) // 下
            move -= mWorldUp;

        if (glm::length(move) > 0.0f)
        {
            move = glm::normalize(move); // 确保对角线速度一致
            // 位置积分: p = p + v * dt
            mCamera->SetPosition(mCamera->GetPosition() + move * mMoveSpeed * deltaTime);
        }
    }

    void CameraController::OnKey(GLFWwindow *window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
    {
        // ESC: 请求关闭窗口
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        // `~`: 切换控制模式
        if (key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS)
        {
            ToggleControl(window);
            return;
        }

        // 记录按键状态(按下/连发 -> true; 释放 -> false)
        if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            SetKeyDown(key, true);
        }
        else if (action == GLFW_RELEASE)
        {
            SetKeyDown(key, false);
        }
    }

    void CameraController::OnCursorPos([[maybe_unused]] GLFWwindow *window, double xpos, double ypos)
    {
        // 非控制模式或未绑定相机则忽略鼠标移动
        if (!mControlEnabled || !mCamera)
            return;

        // 漫游时忽略ImGui捕获鼠标事件, 避免干扰相机导致卡顿
        ImGuiIO &io = ImGui::GetIO();
        if (!mControlEnabled && io.WantCaptureMouse)
            return;

        // 进入控制模式后第一帧只记录坐标, 不计算偏移
        if (mFirstMouse)
        {
            mLastX = xpos;
            mLastY = ypos;
            mFirstMouse = false;
            return;
        }

        const float dx = static_cast<float>(xpos - mLastX);
        const float dy = static_cast<float>(mLastY - ypos); // 屏幕Y向下, 取反符合上移抬头

        mLastX = xpos;
        mLastY = ypos;

        mYaw += dx * mLookSensitivity;
        mPitch += dy * mLookSensitivity;

        // 避免接近 ±90° 时万向节问题/翻转感
        mPitch = glm::clamp(mPitch, -89.0f, 89.0f);

        // 用新角度刷新相机基向量
        UpdateCameraBasisFromYawPitch();
    }

    void CameraController::OnMouseButton([[maybe_unused]] GLFWwindow *window, [[maybe_unused]] int button, [[maybe_unused]] int action, [[maybe_unused]] int mods)
    {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureMouse)
            return;
        // TODO: 处理鼠标点击事件, 鼠标点选, 射线拾取(非漫游模式下)
        if (!mControlEnabled)
        {
            GL_TRACE("Mouse Button Click");
        }
    }

    void CameraController::OnScroll([[maybe_unused]] GLFWwindow *window, [[maybe_unused]] double xoffset, double yoffset)
    {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureMouse)
            return;
        if (!mCamera)
            return;

        // 把滚轮Y偏移交给相机统一处理(透视FOV/正交盒缩放)
        mCamera->Zoom(static_cast<float>(yoffset));
    }

    void CameraController::SetupCallbacks(Application &app, const std::shared_ptr<Camera> &camera)
    {
        // 设置窗口大小回调
        app.SetResizeCallback([camera](uint32_t width, uint32_t height) { // 窗口大小监听
            glViewport(0, 0, width, height);
            if (height != 0)
                camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));
        });

        // 设置输入设备回调
        app.SetKeyCallback([this](GLFWwindow *window, int key, int scancode, int action, int mods) { // 键盘监听
            this->OnKey(window, key, scancode, action, mods);
        });

        app.SetMouseCallback([this](GLFWwindow *window, int button, int action, int mods) { // 鼠标点击监听
            this->OnMouseButton(window, button, action, mods);
        });

        app.SetCursorCallback([this](GLFWwindow *window, double xpos, double ypos) { // 鼠标位置监听
            this->OnCursorPos(window, xpos, ypos);
        });
        
        app.SetScrollCallback([this](GLFWwindow *window, double xoffset, double yoffset) { // 鼠标滚轮监听
            this->OnScroll(window, xoffset, yoffset);
        });
    }
}