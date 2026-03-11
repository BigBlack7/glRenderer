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

void BuildLights()
{
    if (!scene)
        return;

    scene->ClearDirectionalLights();

    core::DirectionalLight mainLight;
    mainLight.SetDirection(glm::vec3(0.f, 0.f, -1.f));
    mainLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    mainLight.SetIntensity(0.9f);
    scene->AddDirectionalLight(mainLight);

    scene->ClearPointLights();
    core::PointLight pointLight;
    pointLight.SetPosition(glm::vec3(-20.f, 0.f, 0.f));
    pointLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    pointLight.SetIntensity(0.9f);
    scene->AddPointLight(pointLight);

    scene->ClearSpotLights();
    core::SpotLight spotLight;
    spotLight.SetPosition(glm::vec3(1.f, 0.f, 0.f));
    spotLight.SetDirection(glm::vec3(-1.f, 0.f, 0.f));
    spotLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    spotLight.SetIntensity(0.9f);
    scene->AddSpotLight(spotLight);
}

void ScenePrepare()
{
    /* shader处理阶段 */
    phongShader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto boxMaterial = std::make_shared<core::Material>(phongShader);

    /* 几何处理阶段 */
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
    if (auto *box = scene->GetEntity(boxID))
    {
        box->SetMesh(cube);
        box->SetMaterial(boxMaterial);
    }

    /* 光源设置阶段 */
    BuildLights();
}

void render()
{
    float time = static_cast<float>(glfwGetTime());

    if (auto *e = scene->GetEntity(boxID))
        // e->GetTransform().SetEulerXyzRad(glm::vec3(0.f, time * 2.f, 0.f));

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
    renderer->SetClearColor(glm::vec4(0.1f, 0.1f, 0.1f, 1.f));
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