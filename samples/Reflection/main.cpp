// core
#include <application/application.hpp>
#include <camera/camera.hpp>
#include <camera/controller.hpp>
#include <entity/entity.hpp>
#include <geometry/mesh.hpp>
#include <geometry/transform.hpp>
#include <material/material.hpp>
#include <shader/shader.hpp>
#include <texture/texture.hpp>
#include <utils/logger.hpp>

std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;
std::shared_ptr<core::Entity> earth = nullptr;
std::shared_ptr<core::Entity> moon = nullptr;
std::shared_ptr<core::Entity> sun = nullptr;

void Prepare()
{
    /* shader处理阶段 */
    auto shader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto earthMaterial = std::make_shared<core::Material>(shader);
    auto moonMaterial = std::make_shared<core::Material>(shader);
    auto sunMaterial = std::make_shared<core::Material>(shader);

    /* 几何处理阶段 */
    auto sphere = core::Mesh::CreateSphere(1.f);

    /* 材质处理阶段 */
    auto earthTex = std::make_shared<core::Texture>("earth.jpg", 0);
    earthMaterial->SetTexture(core::TextureSlot::Albedo, earthTex);
    earthMaterial->SetVec3("uDefaultColor", glm::vec3(1.f, 1.f, 1.f));
    earthMaterial->SetFloat("uShininess", 64.f);

    auto moonTex = std::make_shared<core::Texture>("moon.jpg", 0);
    moonMaterial->SetTexture(core::TextureSlot::Albedo, moonTex);
    moonMaterial->SetVec3("uDefaultColor", glm::vec3(0.23f, 0.43f, 0.82f));
    moonMaterial->SetFloat("uShininess", 32.f);

    auto sunTex = std::make_shared<core::Texture>("sun.jpg", 0);
    sunMaterial->SetTexture(core::TextureSlot::Albedo, sunTex);
    sunMaterial->SetVec3("uDefaultColor", glm::vec3(0.67f, 0.21f, 0.45f));
    sunMaterial->SetFloat("uShininess", 8.f);

    /* 实体构造阶段 */
    earth = std::make_shared<core::Entity>(0u, "Earth");
    earth->SetMesh(sphere);
    earth->SetMaterial(earthMaterial);

    moon = std::make_shared<core::Entity>(1u, "Moon");
    moon->SetMesh(sphere);
    moon->SetMaterial(moonMaterial);
    moon->GetTransform().SetPosition(glm::vec3(2.f, 0.f, 0.f));
    moon->GetTransform().SetScale(glm::vec3(0.27f));

    sun = std::make_shared<core::Entity>(2u, "Sun");
    sun->SetMesh(sphere);
    sun->SetMaterial(sunMaterial);
    sun->GetTransform().SetPosition(glm::vec3(-6.f, 1.f, 0.f));
    sun->GetTransform().SetScale(glm::vec3(2.5f));

    /* 渲染状态设置阶段 */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    earth->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.5f, 0.f));
    earth->Draw(*camera);

    moon->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.5f, 0.f));
    moon->Draw(*camera);

    sun->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.5f, 0.f));
    sun->Draw(*camera);
}

int main()
{
    core::Logger::Init();
    /* ************************************************************************************************ */
    auto &App = core::Application::GetApp();
    constexpr uint32_t WIDTH = 1960;
    constexpr uint32_t HEIGHT = 1080;

    if (!App.Init(WIDTH, HEIGHT))
        return -1;

    camera = std::make_shared<core::Camera>(core::Camera::CreatePerspective(45.f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.f));
    controller = std::make_unique<core::CameraController>();
    controller->SetCamera(camera);

    // 注册各类回调函数
    App.SetResizeCallback([](uint32_t width, uint32_t height) { // 窗口大小监听
        glViewport(0, 0, width, height);
        if (height != 0)
            camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));
    });

    App.SetKeyCallback([](GLFWwindow *window, int key, int scancode, int action, int mods) { // 键盘监听
        controller->OnKey(window, key, scancode, action, mods);
    });

    App.SetMouseCallback([](GLFWwindow *window, int button, int action, int mods) { // 鼠标点击监听
        controller->OnMouseButton(window, button, action, mods);
    });

    App.SetCursorCallback([](GLFWwindow *window, double xpos, double ypos) { // 鼠标位置监听
        controller->OnCursorPos(window, xpos, ypos);
    });

    App.SetScrollCallback([](GLFWwindow *window, double xoffset, double yoffset) { // 鼠标滚轮监听
        controller->OnScroll(window, xoffset, yoffset);
    });

    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0.68f, 0.85f, 0.90f, 1.f);

    Prepare();
    while (App.Update())
    {
        controller->Update(App.GetDeltaTime());

        render();
    }

    App.Destroy();
    /* ************************************************************************************************ */
    core::Logger::Shutdown();
    return 0;
}