#pragma once
#include <glm/glm.hpp>

namespace core
{
    enum class ProjectionType
    {
        Perspective,
        Orthographic
    };

    class Camera
    {
    private:
        glm::vec3 mPosition{0.f, 0.f, 7.f}; // 相机位置
        glm::vec3 mUp{0.f, 1.f, 0.f};       // 相机相机局部上方向
        glm::vec3 mRight{1.f, 0.f, 0.f};    // 相机局部右方向

        ProjectionType mProjectionType{ProjectionType::Perspective};

        // perspective params
        float mFovy{45.f};
        float mAspect{16.f / 9.f};

        // orthographic params
        float mOrthoLeft{-8.f};
        float mOrthoRight{8.f};
        float mOrthoTop{8.f};
        float mOrthoBottom{-8.f};
        float mOrthoScaleExp{0.f}; // 正交相机的缩放比例, 以2为底的指数

        // common params
        float mNear{0.1f};
        float mFar{1000.f};

    public:
        Camera() = default;
        ~Camera() = default;

        /// @brief 创建透视相机
        /// @param fovy 垂直视野角度
        /// @param aspect 宽高比
        /// @param near 近裁剪面距离
        /// @param far 远裁剪面距离
        /// @return 透视相机
        static Camera CreatePerspective(float fovy, float aspect, float near, float far);

        /// @brief 创建正交相机
        /// @param left 左裁剪面距离
        /// @param right 右裁剪面距离
        /// @param top 上裁剪面距离
        /// @param bottom 下裁剪面距离
        /// @param near 近裁剪面距离
        /// @param far 远裁剪面距离
        /// @return 正交相机
        static Camera CreateOrthographic(float left, float right, float top, float bottom, float near, float far);

        /// @brief 滚轮缩放相机(透视=沿front推进/拉远, 正交=改变盒体缩放)
        /// @param wheelDelta 滚轮滚动量
        void Zoom(float wheelDelta);

        // projection config
        void SetProjectionType(ProjectionType type) { mProjectionType = type; }
        ProjectionType GetProjectionType() const { return mProjectionType; }

        /// @brief 获取视图矩阵
        /// @return 视图矩阵
        glm::mat4 GetViewMatrix() const;

        /// @brief 获取投影矩阵
        /// @return 投影矩阵
        glm::mat4 GetProjectionMatrix() const;

        /* getter */
        glm::vec3 GetPosition() const { return mPosition; }
        glm::vec3 GetUp() const { return mUp; }
        glm::vec3 GetRight() const { return mRight; }

        /* setter */
        void SetPosition(const glm::vec3 &position) { mPosition = position; }
        void SetUp(const glm::vec3 &up) { mUp = up; }
        void SetRight(const glm::vec3 &right) { mRight = right; }
        void SetAspect(float aspect) { mAspect = aspect; } // 透视相机可以通过修改aspect来适配窗口大小变化, 让场景保持不变形
    };
}