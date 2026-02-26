#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace core
{
    Camera Camera::CreatePerspective(float fovy, float aspect, float near, float far)
    {
        Camera camera;
        camera.mProjectionType = ProjectionType::Perspective;
        camera.mFovy = fovy;
        camera.mAspect = aspect;
        camera.mNear = near;
        camera.mFar = far;
        return camera;
    }

    Camera Camera::CreateOrthographic(float left, float right, float top, float bottom, float near, float far)
    {
        Camera camera;
        camera.mProjectionType = ProjectionType::Orthographic;
        camera.mOrthoLeft = left;
        camera.mOrthoRight = right;
        camera.mOrthoTop = top;
        camera.mOrthoBottom = bottom;
        camera.mNear = near;
        camera.mFar = far;
        return camera;
    }

    void Camera::Zoom(float wheelDelta)
    {
        if (mProjectionType == ProjectionType::Perspective)
        {
            // 滚轮向上(wheelDelta>0) => 缩小FOV => 放大
            mFovy = glm::clamp(mFovy - wheelDelta * 2.0f, 20.0f, 90.0f);
        }
        else
        {
            // 正交: 缩小盒体 = 放大
            mOrthoScaleExp = glm::clamp(mOrthoScaleExp - wheelDelta * 0.15f, -8.0f, 8.0f);
        }
    }

    glm::mat4 Camera::GetViewMatrix() const
    {
        glm::vec3 front = glm::normalize(glm::cross(mUp, mRight)); // 相机局部前方向
        glm::vec3 center = mPosition + front;                      // 相机看向的目标点
        return glm::lookAt(mPosition, center, mUp);
    }

    glm::mat4 Camera::GetProjectionMatrix() const
    {
        if (mProjectionType == ProjectionType::Perspective)
        {
            return glm::perspective(glm::radians(mFovy), mAspect, mNear, mFar);
        }

        float scale = glm::pow(2.f, mOrthoScaleExp);
        /* 
            正交盒体的边界表示跨国相机视线中心线的左右上下范围,
            同时由于相机看向-Z轴, 故远近平面默认为负,
            同时要注意盒体的范围是否小于相机到场景的距离, 避免被裁剪掉导致场景不可见
        */
        return glm::ortho(mOrthoLeft * scale, mOrthoRight * scale, mOrthoBottom * scale, mOrthoTop * scale, mNear, mFar);
    }
}