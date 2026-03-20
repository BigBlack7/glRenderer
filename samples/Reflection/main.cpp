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
core::EntityID boxID = core::InvalidEntityID;
core::EntityID moonID = core::InvalidEntityID;
core::EntityID sunID = core::InvalidEntityID;

// 光源ID
core::LightID mainDirLightID = core::InvalidLightID;
core::LightID fillDirLightID = core::InvalidLightID;
core::LightID backDirLightID = core::InvalidLightID;
core::LightID topDirLightID = core::InvalidLightID;

void BuildLights()
{
    if (!scene)
        return;

    scene->ClearDirectionalLights();

    core::DirectionalLight mainLight;
    mainLight.SetDirection(glm::vec3(-1.f, -1.f, 0.5f));
    mainLight.SetColor(glm::vec3(1.f, 0.2f, 0.2f));
    mainLight.SetIntensity(0.8f);
    mainDirLightID = scene->CreateDirectionalLight(mainLight);

    core::DirectionalLight fillLight;
    fillLight.SetDirection(glm::vec3(0.8f, -0.6f, 0.5f));
    fillLight.SetColor(glm::vec3(0.2f, 1.f, 0.2f));
    fillLight.SetIntensity(0.6f);
    fillDirLightID = scene->CreateDirectionalLight(fillLight);

    core::DirectionalLight backLight;
    backLight.SetDirection(glm::vec3(0.f, -0.5f, -1.f));
    backLight.SetColor(glm::vec3(0.2f, 0.2f, 1.f));
    backLight.SetIntensity(0.4f);
    backDirLightID = scene->CreateDirectionalLight(backLight);

    core::DirectionalLight topLight;
    topLight.SetDirection(glm::vec3(0.f, -1.f, 0.f));
    topLight.SetColor(glm::vec3(1.f, 1.f, 0.2f));
    topLight.SetIntensity(0.3f);
    topDirLightID = scene->CreateDirectionalLight(topLight);
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
    auto earthTex = std::make_shared<core::Texture>("planet/earth.jpg", 0);
    earthMaterial->SetTexture(core::TextureSlot::Albedo, earthTex);
    earthMaterial->SetBaseColor(glm::vec3(1.f, 1.f, 1.f));
    earthMaterial->SetShininess(64.f);

    auto moonTex = std::make_shared<core::Texture>("planet/moon.jpg", 0);
    moonMaterial->SetTexture(core::TextureSlot::Albedo, moonTex);
    moonMaterial->SetBaseColor(glm::vec3(0.23f, 0.43f, 0.82f));
    moonMaterial->SetShininess(32.f);

    auto sunTex = std::make_shared<core::Texture>("planet/sun.jpg", 0);
    sunMaterial->SetTexture(core::TextureSlot::Albedo, sunTex);
    sunMaterial->SetBaseColor(glm::vec3(0.67f, 0.21f, 0.45f));
    sunMaterial->SetShininess(16.f);

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

    boxID = scene->CreateEntity("Earth");
    if (auto *e = scene->GetEntity(boxID))
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

    scene->Reparent(boxID, sunID);  // 地球挂到太阳
    scene->Reparent(moonID, boxID); // 月亮挂到地球

    /* 光源设置阶段 */
    BuildLights();
}

void render()
{
    float time = static_cast<float>(glfwGetTime());

    if (auto *e = scene->GetEntity(boxID))
        e->GetTransform().SetEulerXyzRad(glm::vec3(0.f, time * 2.f, 0.f));

    if (auto *m = scene->GetEntity(moonID))
        m->GetTransform().SetEulerXyzRad(glm::vec3(0.f, time * 0.1f, 0.f));

    if (auto *s = scene->GetEntity(sunID))
        s->GetTransform().SetEulerXyzRad(glm::vec3(0.f, time * 0.2f, 0.f));

    if (renderer && scene && camera)
        renderer->Render(*scene, *camera, time);
}

int main()
{
    core::Logger::Init();
    /* ************************************************************************************************ */
    auto &App = core::Application::GetApp();
    constexpr uint32_t WIDTH = 1960u;
    constexpr uint32_t HEIGHT = 1080u;

    if (!App.Init(WIDTH, HEIGHT))
        return -1;

    camera = std::make_shared<core::Camera>(core::Camera::CreatePerspective(45.f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.f));
    controller = std::make_unique<core::CameraController>();
    controller->SetCamera(camera);
    controller->SetupCallbacks(App, camera);

    renderer = std::make_unique<core::Renderer>();
    renderer->Init();

    ScenePrepare();
    while (true)
    {
        controller->Update(App.GetDeltaTime());

        render();

        if (!App.Update()) // 避免先交换缓冲区再渲染
            break;
    }

    renderer->Shutdown();
    App.Destroy();
    /* ************************************************************************************************ */
    core::Logger::Shutdown();
    return 0;
}