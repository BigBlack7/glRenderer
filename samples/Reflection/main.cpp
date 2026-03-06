// core
#include <application/application.hpp>
#include <camera/camera.hpp>
#include <camera/controller.hpp>
#include <geometry/mesh.hpp>
#include <geometry/transform.hpp>
#include <material/material.hpp>
#include <scene/entity.hpp>
#include <scene/scene.hpp>
#include <light/light.hpp>
#include <shader/shader.hpp>
#include <texture/texture.hpp>
#include <utils/logger.hpp>

std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;
std::shared_ptr<core::Shader> phongShader = nullptr;

std::unique_ptr<core::Scene> scene = nullptr;
core::EntityID earthID = core::InvalidEntityID;
core::EntityID moonID = core::InvalidEntityID;
core::EntityID sunID = core::InvalidEntityID;

constexpr uint32_t MaxDirectionalLights = 4u;
std::array<core::DirectionalLight, MaxDirectionalLights> directionalLights{};
uint32_t directionalLightCount = 0u;

static constexpr std::array<const char *, MaxDirectionalLights> DirDirectionUniforms{
    "uDirectionalLights[0].direction",
    "uDirectionalLights[1].direction",
    "uDirectionalLights[2].direction",
    "uDirectionalLights[3].direction"};

static constexpr std::array<const char *, MaxDirectionalLights> DirColorUniforms{
    "uDirectionalLights[0].color",
    "uDirectionalLights[1].color",
    "uDirectionalLights[2].color",
    "uDirectionalLights[3].color"};

static constexpr std::array<const char *, MaxDirectionalLights> DirIntensityUniforms{
    "uDirectionalLights[0].intensity",
    "uDirectionalLights[1].intensity",
    "uDirectionalLights[2].intensity",
    "uDirectionalLights[3].intensity"};

void BuildDirectionalLights()
{
    directionalLightCount = 0u;

    // 主光 - 红色, 从右上方照射
    core::DirectionalLight mainLight;
    mainLight.SetDirection(glm::vec3(-1.0f, -1.0f, 0.5f));
    mainLight.SetColor(glm::vec3(1.0f, 0.2f, 0.2f));
    mainLight.SetIntensity(0.8f);
    directionalLights[directionalLightCount++] = mainLight;

    // 补光 - 绿色, 从左前方照射
    core::DirectionalLight fillLight;
    fillLight.SetDirection(glm::vec3(0.8f, -0.6f, 0.5f));
    fillLight.SetColor(glm::vec3(0.2f, 1.0f, 0.2f));
    fillLight.SetIntensity(0.6f);
    directionalLights[directionalLightCount++] = fillLight;

    // 背光 - 蓝色, 从后方照射
    core::DirectionalLight backLight;
    backLight.SetDirection(glm::vec3(0.0f, -0.5f, -1.0f));
    backLight.SetColor(glm::vec3(0.2f, 0.2f, 1.0f));
    backLight.SetIntensity(0.4f);
    directionalLights[directionalLightCount++] = backLight;

    // 顶光 - 黄色, 从正上方照射
    core::DirectionalLight topLight;
    topLight.SetDirection(glm::vec3(0.0f, -1.0f, 0.0f));
    topLight.SetColor(glm::vec3(1.0f, 1.0f, 0.2f));
    topLight.SetIntensity(0.3f);
    directionalLights[directionalLightCount++] = topLight;
}

void UploadDirectionalLights()
{
    if (!phongShader)
        return;

    phongShader->Begin();

    uint32_t uploadCount = 0u;
    for (uint32_t i = 0; i < directionalLightCount && uploadCount < MaxDirectionalLights; ++i)
    {
        const auto &light = directionalLights[i];
        if (!light.IsEnabled())
            continue;

        phongShader->SetVec3(DirDirectionUniforms[uploadCount], light.GetDirection());
        phongShader->SetVec3(DirColorUniforms[uploadCount], light.GetColor());
        phongShader->SetFloat(DirIntensityUniforms[uploadCount], light.GetIntensity());
        ++uploadCount;
    }

    phongShader->SetInt("uDirectionalLightCount", static_cast<int>(uploadCount));
    phongShader->End();
}

void Prepare()
{
    /* shader处理阶段 */
    phongShader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto earthMaterial = std::make_shared<core::Material>(phongShader);
    auto moonMaterial = std::make_shared<core::Material>(phongShader);
    auto sunMaterial = std::make_shared<core::Material>(phongShader);

    /* 几何处理阶段 */
    auto sphere = core::Mesh::CreateCube(1.f);

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
    UploadDirectionalLights();

    /* 渲染状态设置阶段 */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (auto *e = scene->GetEntity(earthID))
        e->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 2.f, 0.f));

    if (auto *m = scene->GetEntity(moonID))
        m->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.1f, 0.f));

    if (auto *s = scene->GetEntity(sunID))
        s->GetTransform().SetEulerXyzRad(glm::vec3(0.f, static_cast<float>(glfwGetTime()) * 0.2f, 0.f));

    scene->UpdateWorldMatrices();

    scene->ForEachRenderable([&](core::EntityID, const core::Entity &entity, const glm::mat4 &world, const glm::mat3 &normal)
                             { entity.Draw(*camera, world, normal); });
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