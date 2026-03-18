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

std::unique_ptr<core::Renderer> renderer = nullptr;
std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;

std::unique_ptr<core::Scene> scene = nullptr;
core::EntityID boxID = core::InvalidEntityID;

core::Entity *objRoot = nullptr;
core::Entity *fbxRoot = nullptr;

// 光源ID
core::LightID mainDirLightID = core::InvalidLightID;
core::LightID pointLightID = core::InvalidLightID;
core::LightID spotLightID = core::InvalidLightID;

// skybox
std::shared_ptr<core::EnvironmentMap> skyboxCube = nullptr;
std::shared_ptr<core::EnvironmentMap> skyboxPanorama = nullptr;
bool skyboxEnabled = true;
int skyboxSource = 0; // 0: cubemap, 1: panorama
float skyboxIntensity = 1.f;

void BuildLights()
{
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

void ScenePrepare()
{
    /* 着色器编译阶段 */
    auto phongShader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto edgeShader = std::make_shared<core::Shader>("effect/edge.vert", "effect/edge.frag");
    auto boxMaterial = std::make_shared<core::Material>(phongShader);
    auto windowMaterial = std::make_shared<core::Material>(phongShader);
    auto edgeMaterial = std::make_shared<core::Material>(edgeShader);

    /* 几何生成阶段 */
    auto cube = core::Mesh::CreateCube(1.f);
    auto plane = core::Mesh::CreatePlane(4.f);

    /* 材质处理阶段 */
    auto boxTex = std::make_shared<core::Texture>("box/box.png", 0);
    auto boxSMTex = std::make_shared<core::Texture>("box/sp_mask.png", 1);
    boxMaterial->SetTexture(core::TextureSlot::Albedo, boxTex);
    boxMaterial->SetTexture(core::TextureSlot::MetallicRoughness, boxSMTex);
    auto boxState = core::MakeOpaqueState();
    boxState.mStencil.mStencilTest = true;
    boxMaterial->SetRenderState(boxState);

    auto windowTex = std::make_shared<core::Texture>("window.png", 0);
    windowMaterial->SetTexture(core::TextureSlot::Albedo, windowTex);
    windowMaterial->SetRenderState(core::MakeTransparentState());

    auto edgeState = core::MakeTransparentState();
    edgeState.mStencil.mStencilTest = true;
    edgeState.mStencil.mFront.mStencilMask = 0x00;
    edgeState.mStencil.mFront.mStencilFunc = core::CompareOp::NotEqual;
    edgeMaterial->SetRenderState(edgeState);

    /* 实体&场景构造阶段 */
    scene = std::make_unique<core::Scene>();

    boxID = scene->CreateEntity("Box");
    auto *box = scene->GetEntity(boxID);
    box->SetMesh(cube);
    box->SetMaterial(boxMaterial);

    auto windowID = scene->CreateEntity("Window");
    auto *window = scene->GetEntity(windowID);
    window->SetMesh(cube);
    window->SetMaterial(windowMaterial);
    window->GetTransform().SetPosition(glm::vec3(0.f, 0.f, 2.f));

    auto edgeID = scene->CreateEntity("Edge");
    auto *edge = scene->GetEntity(edgeID);
    edge->SetMesh(cube);
    edge->SetMaterial(edgeMaterial);
    edge->GetTransform().SetPosition(box->GetTransform().GetPosition());
    edge->GetTransform().SetScale(glm::vec3(1.2f));
    scene->Reparent(edgeID, boxID);

    // OBJ
    core::ModelLoadOptions objOpt{};
    objOpt.__shader__ = phongShader;
    objOpt.__flipTextureY__ = true;
    objOpt.__globalTextureOverrides__[core::TextureSlot::AO] = "ao.jpg";
    auto objModel = core::ModelLoader::Load("bag/backpack.obj", objOpt);
    if (objModel)
    {
        auto objInstance = objModel->Instantiate(*scene, "Backpack");
        auto objState = core::MakeTransparentState();
        objState.mOpacity = 0.5f;
        objModel->ApplyRenderState(*scene, objInstance, objState, {});
        if ((objRoot = scene->GetEntity(objInstance.__rootEntity__)))
        {
            objRoot->GetTransform().SetPosition(glm::vec3(-2.f, 0.f, 0.f));
            objRoot->GetTransform().SetScale(glm::vec3(0.5f));
        }
    }
    // FBX
    core::ModelLoadOptions fbxOpt{};
    fbxOpt.__shader__ = phongShader;
    fbxOpt.__flipTextureY__ = true;
    fbxOpt.__globalTextureOverrides__[core::TextureSlot::Albedo] = "grass/grass.jpg";
    fbxOpt.__globalTextureOverrides__[core::TextureSlot::OpacityMask] = "grass/grassMask.png";
    auto fbxModel = core::ModelLoader::Load("grass.fbx", fbxOpt);
    if (fbxModel)
    {
        auto fbxInstance = fbxModel->Instantiate(*scene, "Grass");
        if ((fbxRoot = scene->GetEntity(fbxInstance.__rootEntity__)))
        {
            fbxRoot->GetTransform().SetPosition(glm::vec3(2.5f, 0.f, 0.f));
            fbxRoot->GetTransform().SetScale(glm::vec3(0.5f));
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
        },
        std::filesystem::path{});

    skyboxPanorama = std::make_shared<core::EnvironmentMap>("skybox/dream.jpg", std::filesystem::path{});

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
glm::vec3 boxPos{0.f, 0.f, 0.f};
glm::vec3 boxRot{0.f, 0.f, 0.f};
glm::vec3 boxScale{1.f, 1.f, 1.f};
glm::vec3 objPos{-2.f, 0.f, 0.f};
glm::vec3 objRot{0.f, 0.f, 0.f};
glm::vec3 objScale{0.5f};
glm::vec3 fbxPos{2.5f, 0.f, 0.f};
glm::vec3 fbxRot{0.f, 0.f, 0.f};
glm::vec3 fbxScale{0.002f};

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
        ImGui::SliderFloat3("Box Position", glm::value_ptr(boxPos), -5.f, 5.f);
        ImGui::SliderFloat3("Box Rotation", glm::value_ptr(boxRot), -180.f, 180.f);
        ImGui::SliderFloat3("Box Scale", glm::value_ptr(boxScale), 0.1f, 5.f);

        ImGui::Separator();
        ImGui::SliderFloat3("Obj Position", glm::value_ptr(objPos), -5.f, 5.f);
        ImGui::SliderFloat3("Obj Rotation", glm::value_ptr(objRot), -180.f, 180.f);
        ImGui::SliderFloat3("Obj Scale", glm::value_ptr(objScale), 0.1f, 5.f);

        ImGui::Separator();
        ImGui::SliderFloat3("Fbx Position", glm::value_ptr(fbxPos), -5.f, 5.f);
        ImGui::SliderFloat3("Fbx Rotation", glm::value_ptr(fbxRot), -180.f, 180.f);
        ImGui::SliderFloat3("Fbx Scale", glm::value_ptr(fbxScale), 0.1f, 5.f);

        ImGui::Separator();
        ImGui::Text("Skybox");
        ImGui::Checkbox("Skybox Enabled", &skyboxEnabled);
        ImGui::SliderFloat("Skybox Intensity", &skyboxIntensity, 0.f, 4.f);

        const char *skyboxItems[] = {"Cubemap (6 Faces)", "Panorama (1 Image)"};
        ImGui::Combo("Skybox Source", &skyboxSource, skyboxItems, IM_ARRAYSIZE(skyboxItems));
        ImGui::End();

        scene->GetEntity(boxID)->GetTransform().SetPosition(boxPos);
        scene->GetEntity(boxID)->GetTransform().SetEulerXyzDeg(boxRot);
        scene->GetEntity(boxID)->GetTransform().SetScale(boxScale);

        objRoot->GetTransform().SetPosition(objPos);
        objRoot->GetTransform().SetEulerXyzDeg(objRot);
        objRoot->GetTransform().SetScale(objScale);

        fbxRoot->GetTransform().SetPosition(fbxPos);
        fbxRoot->GetTransform().SetEulerXyzDeg(fbxRot);
        fbxRoot->GetTransform().SetScale(fbxScale);

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