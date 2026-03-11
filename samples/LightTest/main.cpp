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

// 光源ID管理
core::LightID mainDirLightID = core::InvalidLightID;
core::LightID pointLightID = core::InvalidLightID;
core::LightID spotLightID = core::InvalidLightID;

void BuildLights()
{
    if (!scene)
        return;

    // 清除旧光源
    if (mainDirLightID != core::InvalidLightID)
        scene->RemoveDirectionalLight(mainDirLightID);
    if (pointLightID != core::InvalidLightID)
        scene->RemovePointLight(pointLightID);
    if (spotLightID != core::InvalidLightID)
        scene->RemoveSpotLight(spotLightID);

    // 创建定向光
    core::DirectionalLight mainLight;
    mainLight.SetDirection(glm::vec3(0.f, 0.f, -1.f));
    mainLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    mainLight.SetIntensity(0.9f);
    mainDirLightID = scene->CreateDirectionalLight(mainLight);

    // 创建点光源
    core::PointLight pointLight;
    pointLight.SetPosition(glm::vec3(-20.f, 0.f, 0.f));
    pointLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    pointLight.SetIntensity(0.9f);
    pointLightID = scene->CreatePointLight(pointLight);

    // 创建聚光灯
    core::SpotLight spotLight;
    spotLight.SetPosition(glm::vec3(1.f, 0.f, 0.f));
    spotLight.SetDirection(glm::vec3(-1.f, 0.f, 0.f));
    spotLight.SetColor(glm::vec3(1.f, 1.f, 1.f));
    spotLight.SetIntensity(0.9f);
    spotLightID = scene->CreateSpotLight(spotLight);
}

void UpdateLights(float time)
{
    if (!scene)
        return;
    // 动态更新点光源位置 - 圆形运动
    if (auto *pointLight = scene->GetPointLight(pointLightID))
    {
        float radius = 15.f;
        float speed = 0.5f;
        glm::vec3 newPos(
            radius * glm::cos(time * speed),
            5.f,
            radius * glm::sin(time * speed));
        pointLight->SetPosition(newPos);

        // 根据位置调整颜色
        glm::vec3 color = glm::mix(
            glm::vec3(1.f, 0.5f, 0.2f), // 暖色
            glm::vec3(0.2f, 0.5f, 1.f), // 冷色
            (glm::sin(time * speed * 2.f) + 1.f) * 0.5f);
        pointLight->SetColor(color);
    }
    // 动态更新聚光灯方向 - 摆动
    if (auto *spotLight = scene->GetSpotLight(spotLightID))
    {
        float angle = time * 0.8f;
        glm::vec3 newDir(
            glm::cos(angle),
            -0.5f,
            glm::sin(angle));
        spotLight->SetDirection(glm::normalize(newDir));

        // 根据时间调整强度
        float intensity = 0.7f + 0.3f * glm::sin(time * 2.f);
        spotLight->SetIntensity(intensity);
    }
    // 动态更新定向光方向 - 缓慢旋转
    if (auto *dirLight = scene->GetDirectionalLight(mainDirLightID))
    {
        float angle = time * 0.1f;
        glm::vec3 newDir(
            glm::cos(angle) * 0.7f,
            -0.7f,
            glm::sin(angle) * 0.7f);
        dirLight->SetDirection(glm::normalize(newDir));
    }
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

    // UpdateLights(time);

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