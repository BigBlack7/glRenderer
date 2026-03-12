#include <application/application.hpp>
#include <camera/camera.hpp>
#include <camera/controller.hpp>
#include <geometry/mesh.hpp>
#include <geometry/transform.hpp>
#include <material/material.hpp>
#include <renderer/renderer.hpp>
#include <scene/entity.hpp>
#include <scene/scene.hpp>
#include <light/light.hpp>
#include <shader/shader.hpp>
#include <texture/texture.hpp>
#include <utils/logger.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/gtc/type_ptr.hpp>

std::unique_ptr<core::Renderer> renderer = nullptr;
std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;
std::shared_ptr<core::Shader> phongShader = nullptr;

std::unique_ptr<core::Scene> scene = nullptr;
core::EntityID boxID = core::InvalidEntityID;

// 光源ID
core::LightID mainDirLightID = core::InvalidLightID;
core::LightID pointLightID = core::InvalidLightID;
core::LightID spotLightID = core::InvalidLightID;

void ScenePrepare()
{
    /* 着色器编译阶段 */
    phongShader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto boxMaterial = std::make_shared<core::Material>(phongShader);

    /* 几何生成阶段 */
    auto cube = core::Mesh::CreateCube(1.f);

    /* 材质处理阶段 */
    auto boxTex = std::make_shared<core::Texture>("box/box.png", 0);
    auto boxSMTex = std::make_shared<core::Texture>("box/sp_mask.png", 1);
    boxMaterial->SetTexture(core::TextureSlot::Albedo, boxTex);
    boxMaterial->SetTexture(core::TextureSlot::SpecularMask, boxSMTex);
    boxMaterial->SetVec3("uDefaultColor", glm::vec3(1.f, 1.f, 1.f));
    boxMaterial->SetFloat("uShininess", 4.f);

    /* 实体构造阶段 */
    scene = std::make_unique<core::Scene>();

    boxID = scene->CreateEntity("Box");
    auto *box = scene->GetEntity(boxID);
    box->SetMesh(cube);
    box->SetMaterial(boxMaterial);

    /* 光源设置阶段 */
    core::DirectionalLight mainLight; // 创建定向光
    mainLight.SetDirection(glm::vec3(0.f, 0.f, -1.f));
    mainLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    mainLight.SetIntensity(0.9f);
    mainDirLightID = scene->CreateDirectionalLight(mainLight);

    core::PointLight pointLight; // 创建点光源
    pointLight.SetPosition(glm::vec3(-20.f, 0.f, 0.f));
    pointLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    pointLight.SetIntensity(0.9f);
    pointLightID = scene->CreatePointLight(pointLight);

    core::SpotLight spotLight; // 创建聚光灯
    spotLight.SetPosition(glm::vec3(1.f, 0.f, 0.f));
    spotLight.SetDirection(glm::vec3(-1.f, 0.f, 0.f));
    spotLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    spotLight.SetIntensity(0.9f);
    spotLightID = scene->CreateSpotLight(spotLight);
}

glm::vec4 clearColor{0.68f, 0.85f, 0.90f, 1.f};
glm::vec3 boxPos{0.f, 0.f, 0.f};
void initIMGUI(GLFWwindow *window)
{
    ImGui::CreateContext();   // create imgui memory
    ImGui::StyleColorsDark(); // select theme

    // bind imgui to glfw and opengl
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

int main()
{
    core::Logger::Init();
    /* ************************************************************************************************ */
    auto &App = core::Application::GetApp();
    constexpr uint32_t WIDTH = 1960u;
    constexpr uint32_t HEIGHT = 1080u;
    constexpr float ASPECT_RATIO = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);

    if (!App.Init(WIDTH, HEIGHT))
        return -1;

    camera = std::make_shared<core::Camera>(core::Camera::CreatePerspective(45.f, ASPECT_RATIO, 0.1f, 100.f));
    controller = std::make_unique<core::CameraController>();
    controller->SetCamera(camera);
    controller->SetupCallbacks(App, camera);

    renderer = std::make_unique<core::Renderer>();
    renderer->SetClearColor(glm::vec4(0.1f, 0.1f, 0.1f, 1.f));
    renderer->Init();

    ScenePrepare();
    initIMGUI(App.GetWindow());
    while (true)
    {
        controller->Update(App.GetDeltaTime());

        // 开始ImGui帧 - 每帧开始时
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 构建UI - ImGui帧后Render前
        ImGui::Begin("Settings");
        ImGui::ColorEdit3("Clear Color", glm::value_ptr(clearColor));
        ImGui::SliderFloat3("Box Position", glm::value_ptr(boxPos), -5.f, 5.f);
        ImGui::End();

        scene->GetEntity(boxID)->GetTransform().SetPosition(boxPos);
        renderer->SetClearColor(clearColor);
        renderer->Render(*scene, *camera, static_cast<float>(glfwGetTime()));

        // 渲染ImGui - Render后交换缓冲区前
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!App.Update()) // 避免先交换缓冲区再渲染
            break;
    }

    renderer->Shutdown();
    App.Destroy();
    /* ************************************************************************************************ */
    core::Logger::Shutdown();
    return 0;
}