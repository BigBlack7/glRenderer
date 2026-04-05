#include <application/application.hpp>
#include <camera/camera.hpp>
#include <camera/controller.hpp>
#include <geometry/mesh.hpp>
#include <geometry/transform.hpp>
#include <material/material.hpp>
#include <renderer/renderer.hpp>
#include <scene/loader.hpp>
#include <scene/model.hpp>
#include <scene/entity.hpp>
#include <scene/scene.hpp>
#include <light/light.hpp>
#include <shader/shader.hpp>
#include <texture/texture.hpp>
#include <texture/environmentMap.hpp>
#include <utils/logger.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/gtc/type_ptr.hpp>
#include <array>

std::unique_ptr<core::Renderer> renderer = nullptr;
std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;

std::unique_ptr<core::Scene> scene = nullptr;

// 光源ID
core::LightID pointLightID = core::InvalidLightID;
core::LightID spotLightID = core::InvalidLightID;

int activeLightMode = 0; // 0: point, 1: spot
glm::vec3 lightColor{1.f, 1.f, 1.f};
glm::vec3 lightPosition{0.f, 0.f, 0.f};
glm::vec3 spotDirection{0.f, -1.f, 0.f};
float lightIntensity = 1.f;
float lightRange = 20.f;
float spotInnerAngleDeg = 22.f;
float spotOuterAngleDeg = 35.f;

// skybox
std::shared_ptr<core::EnvironmentMap> skyboxCube = nullptr;
std::shared_ptr<core::EnvironmentMap> skyboxPanorama = nullptr;
bool skyboxEnabled = true;
int skyboxSource = 0; // 0: cubemap, 1: panorama
float skyboxIntensity = 1.f;

void BuildLights()
{
    core::PointLight pointLight; // 创建点光源
    pointLight.SetPosition(lightPosition);
    pointLight.SetColor(lightColor);
    pointLight.SetIntensity(lightIntensity);
    pointLight.SetRange(lightRange);
    pointLight.SetAttenuation(glm::vec3(1.f, 0.09f, 0.032f));
    auto pointShadow = pointLight.GetShadowSettings();
    pointShadow.mEnabled = true;
    pointShadow.mTechnique = core::ShadowTechnique::Hard;
    pointShadow.mBiasConstant = 0.0012f;
    pointShadow.mBiasSlope = 0.004f;
    pointLight.SetShadowSettings(pointShadow);
    pointLightID = scene->CreatePointLight(pointLight);

    core::SpotLight spotLight; // 创建聚光灯
    spotLight.SetPosition(lightPosition);
    spotLight.SetDirection(spotDirection);
    spotLight.SetColor(lightColor);
    spotLight.SetIntensity(lightIntensity);
    spotLight.SetRange(lightRange);
    spotLight.SetInnerAngle(spotInnerAngleDeg);
    spotLight.SetOuterAngle(spotOuterAngleDeg);
    spotLight.SetAttenuation(glm::vec3(1.f, 0.09f, 0.032f));
    auto spotShadow = spotLight.GetShadowSettings();
    spotShadow.mEnabled = true;
    spotShadow.mTechnique = core::ShadowTechnique::PCF;
    spotShadow.mPCFRadiusTexels = 1.f;
    spotShadow.mBiasConstant = 0.0012f;
    spotShadow.mBiasSlope = 0.004f;
    spotLight.SetShadowSettings(spotShadow);
    spotLightID = scene->CreateSpotLight(spotLight);

    if (auto *point = scene->GetPointLight(pointLightID))
        point->SetEnabled(activeLightMode == 0);
    if (auto *spot = scene->GetSpotLight(spotLightID))
        spot->SetEnabled(activeLightMode == 1);
}

void SyncActiveLightSettings()
{
    if (!scene)
        return;

    if (auto *point = scene->GetPointLight(pointLightID))
    {
        point->SetEnabled(activeLightMode == 0);
        point->SetPosition(lightPosition);
        point->SetColor(lightColor);
        point->SetIntensity(lightIntensity);
        point->SetRange(lightRange);
    }

    if (auto *spot = scene->GetSpotLight(spotLightID))
    {
        spot->SetEnabled(activeLightMode == 1);
        spot->SetPosition(lightPosition);
        if (glm::length(spotDirection) > 1e-5f)
            spot->SetDirection(glm::normalize(spotDirection));
        spot->SetColor(lightColor);
        spot->SetIntensity(lightIntensity);
        spot->SetRange(lightRange);
        spot->SetInnerAngle(spotInnerAngleDeg);
        spot->SetOuterAngle(spotOuterAngleDeg);
    }
}

void ScenePrepare()
{
    /* 着色器编译阶段 */
    auto phongShader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto boxMaterial = std::make_shared<core::Material>(phongShader);
    auto bigBoxMaterial = std::make_shared<core::Material>(phongShader);

    /* 几何生成阶段 */
    auto cube = core::Mesh::CreateCube(1.f);
    auto plane = core::Mesh::CreatePlane(10.f);

    /* 材质处理阶段 */
    auto albedoInfo = core::Texture::CreateInfo{.__sRGB__ = true};

    auto boxTex = std::make_shared<core::Texture>("bricks/bricks_albedo.jpg", 0, albedoInfo);
    auto boxNTex = std::make_shared<core::Texture>("bricks/bricks_normal.jpg", 1);
    auto boxHeightTex = std::make_shared<core::Texture>("bricks/bricks_disp.jpg", 2);
    boxMaterial->SetTexture(core::TextureSlot::Albedo, boxTex);
    boxMaterial->SetTexture(core::TextureSlot::Normal, boxNTex);
    boxMaterial->SetTexture(core::TextureSlot::Height, boxHeightTex);

    auto bigBoxTex = std::make_shared<core::Texture>("wall/wall_albedo.jpg", 0, albedoInfo);
    auto bigBoxNTex = std::make_shared<core::Texture>("wall/wall_normal.png", 1);
    bigBoxMaterial->SetTexture(core::TextureSlot::Albedo, bigBoxTex);
    bigBoxMaterial->SetTexture(core::TextureSlot::Normal, bigBoxNTex);

    /* 实体&场景构造阶段 */
    scene = std::make_unique<core::Scene>();

    // 六个plane组成一个边长10的封闭盒子(法线朝内)
    const std::array<const char *, 6> roomPlaneNames = {
        "Room_Back", "Room_Front", "Room_Left", "Room_Right", "Room_Floor", "Room_Ceiling"};
    std::array<core::EntityID, 6> roomPlanes{};
    for (size_t i = 0; i < roomPlaneNames.size(); ++i)
    {
        roomPlanes[i] = scene->CreateEntity(roomPlaneNames[i]);
        if (auto *planeEntity = scene->GetEntity(roomPlanes[i]))
        {
            planeEntity->SetMesh(plane);
            planeEntity->SetMaterial(bigBoxMaterial);
        }
    }

    // Back z=-5, normal +Z
    if (auto *e = scene->GetEntity(roomPlanes[0]))
        e->GetTransform().SetPosition(glm::vec3(0.f, 0.f, -5.f));
    // Front z=+5, normal -Z
    if (auto *e = scene->GetEntity(roomPlanes[1]))
    {
        e->GetTransform().SetPosition(glm::vec3(0.f, 0.f, 5.f));
        e->GetTransform().SetEulerXyzDeg(glm::vec3(0.f, 180.f, 0.f));
    }
    // Left x=-5, normal +X
    if (auto *e = scene->GetEntity(roomPlanes[2]))
    {
        e->GetTransform().SetPosition(glm::vec3(-5.f, 0.f, 0.f));
        e->GetTransform().SetEulerXyzDeg(glm::vec3(0.f, 90.f, 0.f));
    }
    // Right x=+5, normal -X
    if (auto *e = scene->GetEntity(roomPlanes[3]))
    {
        e->GetTransform().SetPosition(glm::vec3(5.f, 0.f, 0.f));
        e->GetTransform().SetEulerXyzDeg(glm::vec3(0.f, -90.f, 0.f));
    }
    // Floor y=-5, normal +Y
    if (auto *e = scene->GetEntity(roomPlanes[4]))
    {
        e->GetTransform().SetPosition(glm::vec3(0.f, -5.f, 0.f));
        e->GetTransform().SetEulerXyzDeg(glm::vec3(-90.f, 0.f, 0.f));
    }
    // Ceiling y=+5, normal -Y
    if (auto *e = scene->GetEntity(roomPlanes[5]))
    {
        e->GetTransform().SetPosition(glm::vec3(0.f, 5.f, 0.f));
        e->GetTransform().SetEulerXyzDeg(glm::vec3(90.f, 0.f, 0.f));
    }

    // 盒子内散落8个边长1的小盒子, 分布更均匀并略向中心收拢
    const std::array<glm::vec3, 8> cubePositions = {
        glm::vec3(-3.8f, -3.6f, -2.7f),
        glm::vec3(3.6f, -3.4f, 2.4f),
        glm::vec3(-3.1f, 3.5f, 2.9f),
        glm::vec3(3.2f, 3.4f, -2.6f),
        glm::vec3(-3.9f, 0.4f, 2.6f),
        glm::vec3(3.8f, -0.6f, -2.8f),
        glm::vec3(0.2f, -3.7f, 3.4f),
        glm::vec3(-0.3f, 3.6f, -3.3f)};

    const std::array<glm::vec3, 8> cubeRotDeg = {
        glm::vec3(0.f, 16.f, 0.f),
        glm::vec3(0.f, -38.f, 0.f),
        glm::vec3(0.f, 58.f, 0.f),
        glm::vec3(0.f, -24.f, 0.f),
        glm::vec3(0.f, 31.f, 0.f),
        glm::vec3(0.f, -67.f, 0.f),
        glm::vec3(0.f, 47.f, 0.f),
        glm::vec3(0.f, -53.f, 0.f)};

    for (size_t i = 0; i < cubePositions.size(); ++i)
    {
        const auto cubeID = scene->CreateEntity("SmallBox_" + std::to_string(i));
        if (auto *e = scene->GetEntity(cubeID))
        {
            e->SetMesh(cube);
            e->SetMaterial(boxMaterial);
            e->GetTransform().SetPosition(cubePositions[i]);
            e->GetTransform().SetEulerXyzDeg(cubeRotDeg[i]);
            e->GetTransform().SetScale(glm::vec3(1.f));
        }
    }

    // OBJ
    core::ModelLoadOptions objOpt{};
    objOpt.__shader__ = phongShader;
    objOpt.__flipTextureY__ = true;
    objOpt.__globalTextureOverrides__[core::TextureSlot::AO] = "ao.jpg";
    auto objModel = core::ModelLoader::Load("bag/backpack.obj", objOpt);
    if (objModel)
    {
        const std::array<glm::vec3, 4> objPositions = {
            glm::vec3(-4.0f, -4.0f, -1.0f),
            glm::vec3(3.8f, -4.0f, 1.2f),
            glm::vec3(-1.1f, -4.0f, 4.0f),
            glm::vec3(1.3f, -4.0f, -4.1f)};
        const std::array<float, 4> objYawDeg = {18.f, -34.f, 72.f, -50.f};
        const std::array<float, 4> objScale = {0.22f, 0.18f, 0.24f, 0.2f};

        for (size_t i = 0; i < objPositions.size(); ++i)
        {
            auto objInstance = objModel->Instantiate(*scene, "Backpack_" + std::to_string(i));
            auto objRootID = objInstance.__rootEntity__;
            if (auto *objRoot = scene->GetEntity(objRootID))
            {
                objRoot->GetTransform().SetPosition(objPositions[i]);
                objRoot->GetTransform().SetEulerXyzDeg(glm::vec3(0.f, objYawDeg[i], 0.f));
                objRoot->GetTransform().SetScale(glm::vec3(objScale[i]));
            }
        }
    }

    /* Skybox阶段 */
    skyboxCube = std::make_shared<core::EnvironmentMap>(
        std::array<std::filesystem::path, 6>{
            "skybox/lake/right.jpg",  // +X
            "skybox/lake/left.jpg",   // -X
            "skybox/lake/top.jpg",    // +Y
            "skybox/lake/bottom.jpg", // -Y
            "skybox/lake/back.jpg",   // +Z
            "skybox/lake/front.jpg"   // -Z
        });

    skyboxPanorama = std::make_shared<core::EnvironmentMap>("skybox/dream.jpg");

    if (skyboxCube && skyboxCube->IsValid())
    {
        skyboxSource = 0;
        scene->SetSkybox(skyboxCube);
        skyboxEnabled = true;
    }
    else if (skyboxPanorama && skyboxPanorama->IsValid())
    {
        skyboxSource = 1;
        scene->SetSkybox(skyboxPanorama);
        skyboxEnabled = true;
    }
    else
    {
        skyboxEnabled = false;
        scene->ClearSkybox();
    }

    scene->SetSkyboxEnabled(skyboxEnabled);
    scene->SetSkyboxIntensity(skyboxIntensity);

    /* 光源设置阶段 */
    BuildLights();
}

glm::vec4 clearColor{0.68f, 0.85f, 0.90f, 1.f};

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

        ImGui::Separator();
        ImGui::Text("Skybox");
        ImGui::Checkbox("Skybox Enabled", &skyboxEnabled);
        ImGui::SliderFloat("Skybox Intensity", &skyboxIntensity, 0.f, 4.f);

        const char *skyboxItems[] = {"Cubemap (6 Faces)", "Panorama (1 Image)"};
        ImGui::Combo("Skybox Source", &skyboxSource, skyboxItems, IM_ARRAYSIZE(skyboxItems));

        ImGui::Separator();
        ImGui::Text("Light");
        const char *lightModes[] = {"Point", "Spot"};
        ImGui::Combo("Light Mode", &activeLightMode, lightModes, IM_ARRAYSIZE(lightModes));
        ImGui::ColorEdit3("Light Color", glm::value_ptr(lightColor));
        ImGui::SliderFloat3("Light Position", glm::value_ptr(lightPosition), -4.5f, 4.5f);
        ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.f, 8.f);
        ImGui::SliderFloat("Light Range", &lightRange, 2.f, 40.f);
        if (activeLightMode == 1)
        {
            ImGui::SliderFloat3("Spot Direction", glm::value_ptr(spotDirection), -1.f, 1.f);
            ImGui::SliderFloat("Spot Inner Angle", &spotInnerAngleDeg, 5.f, 70.f);
            ImGui::SliderFloat("Spot Outer Angle", &spotOuterAngleDeg, 10.f, 85.f);
            if (spotOuterAngleDeg < spotInnerAngleDeg + 1.f)
                spotOuterAngleDeg = spotInnerAngleDeg + 1.f;
        }
        ImGui::End();

        if (skyboxSource == 0)
        {
            if (skyboxCube && skyboxCube->IsValid())
                scene->SetSkybox(skyboxCube);
            else
                skyboxEnabled = false;
        }
        else
        {
            if (skyboxPanorama && skyboxPanorama->IsValid())
                scene->SetSkybox(skyboxPanorama);
            else
                skyboxEnabled = false;
        }

        scene->SetSkyboxEnabled(skyboxEnabled);
        scene->SetSkyboxIntensity(skyboxIntensity);
        SyncActiveLightSettings();

        renderer->SetClearColor(clearColor);
        renderer->Render(*scene, *camera, static_cast<float>(glfwGetTime()));

        // 渲染ImGui - Render后交换缓冲区前
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!App.Update()) // 避免先交换缓冲区再渲染
            break;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    renderer->Shutdown();
    App.Destroy();
    /* ************************************************************************************************ */
    core::Logger::Shutdown();
    return 0;
}