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

std::unique_ptr<core::Renderer> renderer = nullptr;
std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;
std::shared_ptr<core::Shader> phongShader = nullptr;

std::unique_ptr<core::Scene> scene = nullptr;
core::EntityID earthID = core::InvalidEntityID;
core::EntityID moonID = core::InvalidEntityID;
core::EntityID sunID = core::InvalidEntityID;

void BuildDirectionalLights()
{
    if (!scene)
        return;

    scene->ClearDirectionalLights();

    core::DirectionalLight mainLight;
    mainLight.SetDirection(glm::vec3(-1.0f, -1.0f, 0.5f));
    mainLight.SetColor(glm::vec3(1.0f, 0.2f, 0.2f));
    mainLight.SetIntensity(0.8f);
    scene->AddDirectionalLight(mainLight);

    core::DirectionalLight fillLight;
    fillLight.SetDirection(glm::vec3(0.8f, -0.6f, 0.5f));
    fillLight.SetColor(glm::vec3(0.2f, 1.0f, 0.2f));
    fillLight.SetIntensity(0.6f);
    scene->AddDirectionalLight(fillLight);

    core::DirectionalLight backLight;
    backLight.SetDirection(glm::vec3(0.0f, -0.5f, -1.0f));
    backLight.SetColor(glm::vec3(0.2f, 0.2f, 1.0f));
    backLight.SetIntensity(0.4f);
    scene->AddDirectionalLight(backLight);

    core::DirectionalLight topLight;
    topLight.SetDirection(glm::vec3(0.0f, -1.0f, 0.0f));
    topLight.SetColor(glm::vec3(1.0f, 1.0f, 0.2f));
    topLight.SetIntensity(0.3f);
    scene->AddDirectionalLight(topLight);
}

void ScenePrepare()
{
    /* shader处理阶段 */
    phongShader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto earthMaterial = std::make_shared<core::Material>(phongShader);
    auto moonMaterial = std::make_shared<core::Material>(phongShader);
    auto sunMaterial = std::make_shared<core::Material>(phongShader);

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
    sunMaterial->SetFloat("uShininess", 16.f);

    /* 实体构造阶段 */
    scene = std::make_unique<core::Scene>();

    sunID = scene->CreateEntity("Sun");
    if (auto *s = scene->GetEntity(sunID))
    {
        s->SetMesh(sphere);
        s->SetMaterial(sunMaterial);
        s->GetTransform().SetPosition(glm::vec3(0.0f, 0.f, 0.f));
        s->GetTransform().SetScale(glm::vec3(1.f));
    }

    earthID = scene->CreateEntity("Earth");
    if (auto *e = scene->GetEntity(earthID))
    {
        e->SetMesh(sphere);
        e->SetMaterial(earthMaterial);
        e->GetTransform().SetPosition(glm::vec3(3.5f, 0.0f, 0.0f));
        e->GetTransform().SetScale(glm::vec3(0.3f));
    }

    moonID = scene->CreateEntity("Moon");
    if (auto *m = scene->GetEntity(moonID))
    {
        m->SetMesh(sphere);
        m->SetMaterial(moonMaterial);
        m->GetTransform().SetPosition(glm::vec3(2.f, 0.f, 0.f));
        m->GetTransform().SetScale(glm::vec3(0.1f));
    }

    scene->Reparent(earthID, sunID);  // 地球挂到太阳
    scene->Reparent(moonID, earthID); // 月亮挂到地球

    /* 光源设置阶段 */
    BuildDirectionalLights();
}

void render()
{
    if (auto *e = scene->GetEntity(earthID))
        e->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 2.f, 0.f));

    if (auto *m = scene->GetEntity(moonID))
        m->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.1f, 0.f));

    if (auto *s = scene->GetEntity(sunID))
        s->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.2f, 0.f));

    if (renderer && scene && camera)
        renderer->Render(*scene, *camera, static_cast<float>(glfwGetTime()));
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
    controller->SetupCallbacks(App, camera);

    renderer = std::make_unique<core::Renderer>();
    renderer->SetClearColor(glm::vec4(0.68f, 0.85f, 0.90f, 1.f));
    renderer->Init();

    ScenePrepare();

    while (true)
    {
        controller->Update(App.GetDeltaTime());

        render();

        if (!App.Update()) // 避免先交换缓冲区再渲染
            break;
    }

    if (renderer)
        renderer->Shutdown();

    App.Destroy();
    /* ************************************************************************************************ */
    core::Logger::Shutdown();
    return 0;
}