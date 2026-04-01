#pragma once
#include "camera.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <array>

namespace core
{
    class CameraController
    {
    private:
        static constexpr int KeyCount = GLFW_KEY_LAST + 1; // 键值范围: [0, GLFW_KEY_LAST]

        std::shared_ptr<Camera> mCamera{};
        std::array<uint8_t, KeyCount> mKeyStatus{}; // 默认全0 (0: up, 1: down)

        bool mControlEnabled{false}; // `~` 开关
        bool mFirstMouse{true};      // 首帧鼠标标记: 进入控制模式后第一帧用于对齐, 避免视角突跳
        double mLastX{0.0};          // 上一帧鼠标X位置
        double mLastY{0.0};          // 上一帧鼠标Y位置

        float mMoveSpeed{3.0f};        // 移动速度: WASD/上下箭头位移速度
        float mLookSensitivity{0.05f}; // 视角灵敏度: 鼠标每移动1像素, 偏航/俯仰变化对应度

        float mYaw{-90.0f};                // 偏航, 左右看(初始值表示默认朝向-Z)
        float mPitch{0.0f};                // 俯仰, 上下看
        glm::vec3 mWorldUp{0.f, 1.f, 0.f}; // 世界坐标系中的上方向, 用于计算相机的右方向和前方向

    private:
        /// @brief 切换控制状态, 开启控制则隐藏并锁定鼠标, 关闭控制恢复鼠标显示与移动
        /// @param window 窗口指针
        void ToggleControl(GLFWwindow *window);

        /// @brief 根据 yaw/pitch 计算 front/right/up, 并写回相机
        void UpdateCameraBasisFromYawPitch();

        /// @brief 同步相机旋转角度, 常用于 SetCamera 后同步, 避免控制器角度与相机实际姿态不一致
        void SyncYawPitchFromCamera();

        bool IsKeyValid(int key) const { return key >= 0 && key < KeyCount; }             // 检查键值是否有效
        bool IsKeyDown(int key) const { return IsKeyValid(key) && mKeyStatus[key] != 0; } // 检查键是否按下
        void SetKeyDown(int key, bool down)                                               // 设置键是否按下
        {
            if (IsKeyValid(key))
                mKeyStatus[key] = down ? 1 : 0;
        }

    public:
        /// @brief 设置控制的相机
        /// @param camera 相机指针
        void SetCamera(const std::shared_ptr<Camera> &camera);

        /// @brief 更新相机状态, 根据按键状态驱动位移
        /// @param deltaTime 时间间隔
        void Update(float deltaTime);

        /// @brief 处理输入事件
        /// @param window 窗口指针
        /// @param key 键值
        /// @param scancode 扫描码
        /// @param action 操作类型
        /// @param mods 修饰键状态
        void OnKey(GLFWwindow *window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods);

        /// @brief 处理鼠标位置移动事件
        /// @param window 窗口指针
        /// @param xpos 鼠标X位置
        /// @param ypos 鼠标Y位置
        void OnCursorPos([[maybe_unused]] GLFWwindow *window, double xpos, double ypos);

        /// @brief 处理鼠标按钮事件
        /// @param window 窗口指针
        /// @param button 鼠标按钮
        /// @param action 操作类型
        /// @param mods 修饰键状态
        void OnMouseButton(GLFWwindow *window, int button, int action, int mods);

        /// @brief 处理鼠标滚轮事件
        /// @param window 窗口指针
        /// @param xoffset 滚轮X偏移量
        /// @param yoffset 滚轮Y偏移量
        void OnScroll([[maybe_unused]] GLFWwindow *window, [[maybe_unused]] double xoffset, double yoffset);

        /// @brief 设置所有输入回调函数
        /// @param app 应用程序实例
        /// @param camera 相机实例(用于resize回调)
        void SetupCallbacks(class Application &app, const std::shared_ptr<Camera> &camera);

        bool IsControlEnabled() const { return mControlEnabled; }

        void SetMoveSpeed(float speed) { mMoveSpeed = speed; }
        void SetLookSensitivity(float sensitivity) { mLookSensitivity = sensitivity; }
    };
}