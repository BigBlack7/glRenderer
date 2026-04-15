#include <application/application.hpp>
#include <camera/camera.hpp>
#include <camera/controller.hpp>
#include <geometry/mesh.hpp>
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
#include <glm/geometric.hpp>
#include <string>

std::unique_ptr<core::Renderer> renderer = nullptr;
std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;

std::unique_ptr<core::Scene> scene = nullptr;
core::EntityID sphereID = core::InvalidEntityID;
core::EntityID modelRootID = core::InvalidEntityID;
core::EntityID groundID = core::InvalidEntityID;
std::shared_ptr<core::Material> sphereMaterial = nullptr;

// 光源ID
core::LightID dirLightID = core::InvalidLightID;
core::LightShadowSettings dirShadowSettings{};

// skybox
std::shared_ptr<core::EnvironmentMap> skyboxCube = nullptr;
std::shared_ptr<core::EnvironmentMap> skyboxPanorama = nullptr;
bool skyboxEnabled = true;
int skyboxSource = 0; // 0: garden.exr, 1: room.exr
float skyboxIntensity = 1.f;
int shadowTechnique = static_cast<int>(core::ShadowTechnique::PoissonPCF);

core::PostProcessSettings postSettings{};

glm::vec3 sphereAlbedo{1.0f, 0.3f, 0.1f};
float sphereMetallic = 0.1f;
float sphereRoughness = 0.45f;
float sphereNormalStrength = 1.0f;

glm::vec3 spherePos{0.f, 2.f, 0.f};
glm::vec3 modelPos{-3.f, -0.6f, 0.f};
glm::vec3 dirLightDirection{-1.f, -1.f, -1.f};
glm::vec3 dirLightColor{1.0f, 0.98f, 0.95f};
float dirLightIntensity = 1.1f;

void BuildLights()
{
    core::DirectionalLight dirLight;
    dirLight.SetDirection(glm::normalize(dirLightDirection));
    dirLight.SetColor(dirLightColor);
    dirLight.SetIntensity(dirLightIntensity);

    dirShadowSettings.mEnabled = true;
    dirShadowSettings.mTechnique = static_cast<core::ShadowTechnique>(shadowTechnique);
    // Reduce self-shadow acne on smooth curved surfaces.
    dirShadowSettings.mBiasConstant = 0.0025f;
    dirShadowSettings.mBiasSlope = 0.01f;
    dirShadowSettings.mPoissonRadiusUV = 0.0015f;
    dirShadowSettings.mPoissonSampleCount = 16u;

    dirLight.SetShadowSettings(dirShadowSettings);
    dirLightID = scene->CreateDirectionalLight(dirLight);
}

void ScenePrepare()
{
    auto pbrShader = std::make_shared<core::Shader>("pbr/pbr.vert", "pbr/pbr.frag");
    auto phongShader = std::make_shared<core::Shader>("phong/phong.vert", "phong/phong.frag");
    auto sphereMesh = core::Mesh::CreateSphere(0.9f, 64u, 64u);
    auto planeMesh = core::Mesh::CreatePlane(18.f);

    scene = std::make_unique<core::Scene>();

    sphereMaterial = std::make_shared<core::Material>(pbrShader);
    sphereMaterial->SetBaseColor(sphereAlbedo);
    sphereMaterial->SetFloat("uMetallic", sphereMetallic);
    sphereMaterial->SetFloat("uRoughness", sphereRoughness);
    sphereMaterial->SetFloat("uNormalStrength", sphereNormalStrength);

    sphereID = scene->CreateEntity("PBR_MaterialSphere");
    if (auto *sphereEntity = scene->GetEntity(sphereID))
    {
        sphereEntity->SetMesh(sphereMesh);
        sphereEntity->SetMaterial(sphereMaterial);
        sphereEntity->GetTransform().SetPosition(spherePos);
    }

    auto albedoInfo = core::Texture::CreateInfo{.__sRGB__ = true};
    auto tileTex = std::make_shared<core::Texture>("tile/slab_tiles_diff_2k.jpg", 0, albedoInfo);
    auto tileNTex = std::make_shared<core::Texture>("tile/slab_tiles_nor_gl_2k.jpg", 1);
    auto tileARMTex = std::make_shared<core::Texture>("tile/slab_tiles_arm_2k.jpg", 2);
    auto tileMaterial = std::make_shared<core::Material>(pbrShader);
    tileMaterial->SetTexture(core::TextureSlot::Albedo, tileTex);
    tileMaterial->SetTexture(core::TextureSlot::Normal, tileNTex);
    tileMaterial->SetTexture(core::TextureSlot::MetallicRoughness, tileARMTex);
    auto tileID = scene->CreateEntity("Tile");
    if (auto *tileEntity = scene->GetEntity(tileID))
    {
        tileEntity->SetMesh(planeMesh);
        tileEntity->SetMaterial(tileMaterial);
        tileEntity->GetTransform().SetPosition(glm::vec3{0.f, -2.f, 0.f});
        tileEntity->GetTransform().SetEulerXyzDeg(glm::vec3(-90.f, 0.f, 0.f));
    }

    core::ModelLoadOptions objOpt{};
    objOpt.__shader__ = pbrShader;
    objOpt.__flipTextureY__ = true;
    auto objModel = core::ModelLoader::Load("bag/backpack.obj", objOpt);
    if (objModel)
    {
        auto modelInstance = objModel->Instantiate(*scene, "PBR_Model");
        modelRootID = modelInstance.__rootEntity__;
        if (auto *objRoot = scene->GetEntity(modelRootID))
        {
            objRoot->GetTransform().SetPosition(modelPos);
            objRoot->GetTransform().SetEulerXyzDeg(glm::vec3(0.f, 24.f, 0.f));
            objRoot->GetTransform().SetScale(glm::vec3(0.55f));
        }
    }

    core::ModelLoadOptions fbxOpt{};
    fbxOpt.__shader__ = pbrShader;
    fbxOpt.__flipTextureY__ = true;
    auto fbxModel = core::ModelLoader::Load("fist.fbx", fbxOpt);
    if (fbxModel)
    {
        constexpr int gridSize = 4;
        constexpr float spacing = 1.8f;
        const glm::vec3 gridCenter{2.2f, -1.4f, 0.f};
        const glm::vec3 fistScale{0.006f};

        const float half = 0.5f * static_cast<float>(gridSize - 1);
        for (int row = 0; row < gridSize; ++row)
        {
            for (int col = 0; col < gridSize; ++col)
            {
                auto fbxInstance = fbxModel->Instantiate(*scene, "Fist_" + std::to_string(row) + "_" + std::to_string(col));
                if (auto *fbxRoot = scene->GetEntity(fbxInstance.__rootEntity__))
                {
                    const float x = (static_cast<float>(col) - half) * spacing;
                    const float z = (static_cast<float>(row) - half) * spacing;
                    fbxRoot->GetTransform().SetPosition(gridCenter + glm::vec3(x, 0.f, z));
                    fbxRoot->GetTransform().SetScale(fistScale);
                }
            }
        }
    }

    skyboxCube = std::make_shared<core::EnvironmentMap>("grasslands.exr");
    skyboxPanorama = std::make_shared<core::EnvironmentMap>("room.exr");

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
    postSettings.mToneMapMode = core::ToneMapMode::Exposure;
    postSettings.mExposure = 1.2f;
    postSettings.mBloomEnabled = true;
    postSettings.mBloomIntensity = 0.07f;
    postSettings.mBloomThreshold = 1.0f;
    postSettings.mBloomIterations = 6u;
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
        ImGui::Text("PBR Sphere");
        const bool sphereAnyTexture = sphereMaterial && sphereMaterial->GetFeatureFlags() != 0u;
        if (sphereAnyTexture)
            ImGui::TextUnformatted("Sphere has texture(s), parameters locked.");

        ImGui::BeginDisabled(sphereAnyTexture);
        ImGui::ColorEdit3("Albedo", glm::value_ptr(sphereAlbedo));
        ImGui::SliderFloat("Normal Strength", &sphereNormalStrength, 0.f, 2.f, "%.2f");
        ImGui::SliderFloat("Metallic", &sphereMetallic, 0.f, 1.f, "%.3f");
        ImGui::SliderFloat("Roughness", &sphereRoughness, 0.04f, 1.f, "%.3f");
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Text("Skybox");
        ImGui::Checkbox("Skybox Enabled", &skyboxEnabled);
        ImGui::SliderFloat("Skybox Intensity", &skyboxIntensity, 0.f, 4.f);

        const char *skyboxItems[] = {"grasslands.exr", "room.exr"};
        ImGui::Combo("Skybox Source", &skyboxSource, skyboxItems, IM_ARRAYSIZE(skyboxItems));

        ImGui::Separator();
        ImGui::Text("Directional Light");
        ImGui::ColorEdit3("Light Color", glm::value_ptr(dirLightColor));
        ImGui::SliderFloat3("Light Direction", glm::value_ptr(dirLightDirection), -1.f, 1.f);
        ImGui::SliderFloat("Light Intensity", &dirLightIntensity, 0.f, 4.f);

        ImGui::Separator();
        ImGui::Text("Directional Shadow");
        ImGui::Checkbox("Shadow Enabled", &dirShadowSettings.mEnabled);
        const char *shadowTechniqueItems[] = {"None", "Hard", "PCF", "Poisson PCF", "PCSS", "CSM"};
        if (ImGui::Combo("Shadow Technique", &shadowTechnique, shadowTechniqueItems, IM_ARRAYSIZE(shadowTechniqueItems)))
            dirShadowSettings.mTechnique = static_cast<core::ShadowTechnique>(shadowTechnique);

        ImGui::SliderFloat("Shadow Bias Constant", &dirShadowSettings.mBiasConstant, 0.0002f, 0.01f, "%.6f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Shadow Bias Slope", &dirShadowSettings.mBiasSlope, 0.0005f, 0.04f, "%.6f", ImGuiSliderFlags_Logarithmic);

        if (shadowTechnique == static_cast<int>(core::ShadowTechnique::PCF))
        {
            ImGui::SliderFloat("PCF Radius Texels", &dirShadowSettings.mPCFRadiusTexels, 0.0f, 4.0f, "%.2f");
        }
        else if (shadowTechnique == static_cast<int>(core::ShadowTechnique::PoissonPCF))
        {
            ImGui::SliderFloat("Poisson Radius UV", &dirShadowSettings.mPoissonRadiusUV, 0.0001f, 0.02f, "%.5f", ImGuiSliderFlags_Logarithmic);
            int poissonSamplesUI = static_cast<int>(dirShadowSettings.mPoissonSampleCount);
            if (ImGui::SliderInt("Poisson Samples", &poissonSamplesUI, 1, 32))
                dirShadowSettings.mPoissonSampleCount = static_cast<uint32_t>(poissonSamplesUI);
        }
        else if (shadowTechnique == static_cast<int>(core::ShadowTechnique::PCSS))
        {
            ImGui::SliderFloat("Blocker Search Texels", &dirShadowSettings.mPCSSBlockerSearchTexels, 0.5f, 12.0f, "%.2f");
            ImGui::SliderFloat("Light Size Texels", &dirShadowSettings.mPCSSLightSizeTexels, 1.0f, 64.0f, "%.2f");
            ImGui::SliderFloat("Min Filter Texels", &dirShadowSettings.mPCSSMinFilterTexels, 0.0f, 8.0f, "%.2f");
            ImGui::SliderFloat("Max Filter Texels", &dirShadowSettings.mPCSSMaxFilterTexels, 1.0f, 96.0f, "%.2f");

            int blockerSamplesUI = static_cast<int>(dirShadowSettings.mPCSSBlockerSampleCount);
            if (ImGui::SliderInt("Blocker Samples", &blockerSamplesUI, 1, 32))
                dirShadowSettings.mPCSSBlockerSampleCount = static_cast<uint32_t>(blockerSamplesUI);

            int filterSamplesUI = static_cast<int>(dirShadowSettings.mPCSSFilterSampleCount);
            if (ImGui::SliderInt("Filter Samples", &filterSamplesUI, 1, 32))
                dirShadowSettings.mPCSSFilterSampleCount = static_cast<uint32_t>(filterSamplesUI);
        }
        else if (shadowTechnique == static_cast<int>(core::ShadowTechnique::CSM))
        {
            int cascadeCountUI = static_cast<int>(dirShadowSettings.mCascadeCount);
            if (ImGui::SliderInt("Cascade Count", &cascadeCountUI, 2, 4))
                dirShadowSettings.mCascadeCount = static_cast<uint32_t>(cascadeCountUI);

            ImGui::SliderFloat("Split Exponent", &dirShadowSettings.mCascadeSplitExponent, 1.1f, 5.0f, "%.2f");
            ImGui::SliderFloat("Cascade Blend", &dirShadowSettings.mCascadeBlend, 0.0f, 0.5f, "%.3f");
            ImGui::SliderFloat("Far Resolution Scale", &dirShadowSettings.mCascadeFarResolutionScale, 0.2f, 1.0f, "%.2f");
            ImGui::SliderFloat("CSM PCF Radius", &dirShadowSettings.mPCFRadiusTexels, 0.0f, 4.0f, "%.2f");
        }

        ImGui::Separator();
        ImGui::Text("Post Process");
        int toneMapModeUI = static_cast<int>(postSettings.mToneMapMode);
        const char *toneMapItems[] = {"Reinhard", "Exposure"};
        if (ImGui::Combo("Tone Mapping", &toneMapModeUI, toneMapItems, IM_ARRAYSIZE(toneMapItems)))
            postSettings.mToneMapMode = static_cast<core::ToneMapMode>(toneMapModeUI);

        if (postSettings.mToneMapMode == core::ToneMapMode::Exposure)
            ImGui::SliderFloat("Exposure", &postSettings.mExposure, 0.01f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);

        ImGui::Checkbox("Gamma Enabled", &postSettings.mGammaEnabled);
        ImGui::SliderFloat("Gamma", &postSettings.mGamma, 1.0f, 3.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("Bloom");
        ImGui::Checkbox("Bloom Enabled", &postSettings.mBloomEnabled);
        ImGui::SliderFloat("Bloom Intensity", &postSettings.mBloomIntensity, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Bloom Threshold", &postSettings.mBloomThreshold, 0.1f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Bloom Knee", &postSettings.mBloomKnee, 0.01f, 2.0f, "%.2f");
        int bloomIterUI = static_cast<int>(postSettings.mBloomIterations);
        if (ImGui::SliderInt("Bloom Iterations", &bloomIterUI, 1, 16))
            postSettings.mBloomIterations = static_cast<uint32_t>(bloomIterUI);
        ImGui::End();

        if (sphereMaterial)
        {
            sphereMaterial->SetBaseColor(sphereAlbedo);
            sphereMaterial->SetFloat("uNormalStrength", sphereNormalStrength);
            sphereMaterial->SetFloat("uMetallic", sphereMetallic);
            sphereMaterial->SetFloat("uRoughness", sphereRoughness);
        }

        if (auto *sphereEntity = scene->GetEntity(sphereID))
        {
            sphereEntity->GetTransform().SetPosition(spherePos);
        }

        if (auto *objRoot = scene->GetEntity(modelRootID))
        {
            objRoot->GetTransform().SetPosition(modelPos);
        }

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

        if (auto *dir = scene->GetDirectionalLight(dirLightID))
        {
            if (glm::dot(dirLightDirection, dirLightDirection) < 1e-6f)
                dirLightDirection = glm::vec3(-1.f, -1.f, -1.f);

            dir->SetDirection(glm::normalize(dirLightDirection));
            dir->SetColor(dirLightColor);
            dir->SetIntensity(dirLightIntensity);
            dirShadowSettings.mTechnique = static_cast<core::ShadowTechnique>(shadowTechnique);
            dir->SetShadowSettings(dirShadowSettings);
        }

        renderer->SetPostProcessSettings(postSettings);

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