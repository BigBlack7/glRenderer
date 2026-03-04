// core
#include <application/application.hpp>
#include <camera/camera.hpp>
#include <camera/controller.hpp>
#include <geometry/mesh.hpp>
#include <geometry/transform.hpp>
#include <material/material.hpp>
#include <shader/shader.hpp>
#include <texture/texture.hpp>
#include <utils/logger.hpp>

std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;
std::shared_ptr<core::Material> material = nullptr;
std::unique_ptr<core::Mesh> geometry = nullptr;
core::Transform modelTrans{};

void Prepare()
{
    /* shader处理阶段 */
    auto shader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    material = std::make_shared<core::Material>(shader);

    /* 几何处理阶段 */
    geometry = core::Mesh::CreateSphere(1.f);

    /* 材质处理阶段 */
    auto earthTex = std::make_shared<core::Texture>("earth.png", 0);
    material->SetTexture(core::TextureSlot::Albedo, earthTex);
    material->SetVec3("uDefaultColor", glm::vec3(1.f, 1.f, 1.f));
    material->SetFloat("uShininess", 64.f);

    /* 渲染状态设置阶段 */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    material->Bind();
    const auto &shader = material->GetShader();

    shader->SetMat4("uP", camera->GetProjectionMatrix());
    shader->SetMat4("uV", camera->GetViewMatrix());
    modelTrans.SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.5f, 0.f));
    shader->SetMat4("uM", modelTrans.GetModelMatrix());
    shader->SetMat3("uN", modelTrans.GetNormalMatrix());
    shader->SetVec3("uViewPos", camera->GetPosition());

    geometry->Draw();

    material->Unbind();
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
        {
            camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));
        }
    });

    App.SetKeyCallback([](GLFWwindow *window, int key, int scancode, int action, int mods) { // 键盘监听
        controller->OnKey(window, key, scancode, action, mods);
    });

    App.SetMouseCallback([](GLFWwindow *window, int button, int action, int mods) { // 鼠标点击监听
        // TODO 鼠标点击事件
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