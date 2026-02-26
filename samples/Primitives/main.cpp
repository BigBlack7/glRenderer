// core
#include <application/application.hpp>
#include <camera/camera.hpp>
#include <camera/cameraController.hpp>
#include <shader/shader.hpp>
#include <texture/texture.hpp>
#include <utils/logger.hpp>

std::shared_ptr<core::Camera> camera = nullptr;
std::unique_ptr<core::CameraController> controller = nullptr;
std::unique_ptr<core::Shader> shader = nullptr;
std::unique_ptr<core::Texture> grass = nullptr;
std::unique_ptr<core::Texture> land = nullptr;
std::unique_ptr<core::Texture> noise = nullptr;

void Prepare(GLuint &vao)
{
    /* shader处理阶段 */
    shader = std::make_unique<core::Shader>("primary/triangle.vert", "primary/triangle.frag");

    /* 数据处理阶段 */
    float postions[] = {
        -0.5f, -0.5f, 0.f,
        0.5f, -0.5f, 0.f,
        -0.5f, 0.5f, 0.f,
        0.5f, 0.5f, 0.f};

    float colors[] = {
        1.f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f,
        0.5f, 0.5f, 0.5f};

    float uv[] = {
        0.f, 0.f,
        1.f, 0.f,
        0.f, 1.f,
        1.f, 1.f};

    uint8_t indices[] = {
        0, 1, 2,
        2, 1, 3};

    // 顶点缓冲对象(VBO)和元素缓冲对象(EBO)
    GLuint posVBO = 0;
    glGenBuffers(1, &posVBO);
    glBindBuffer(GL_ARRAY_BUFFER, posVBO);                                     // 绑定顶点缓冲对象, 后续的操作都将对该缓冲对象进行操作
    glBufferData(GL_ARRAY_BUFFER, sizeof(postions), postions, GL_STATIC_DRAW); // 开辟显存, 传输数据到显存中

    GLuint colorVBO = 0;
    glGenBuffers(1, &colorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);

    GLuint uvVBO = 0;
    glGenBuffers(1, &uvVBO);
    glBindBuffer(GL_ARRAY_BUFFER, uvVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uv), uv, GL_STATIC_DRAW);

    GLuint ebo = 0;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 顶点数组对象(VAO)
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, posVBO);
    glEnableVertexAttribArray(0); // 启用顶点属性数组, 0为顶点属性位置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, uvVBO);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindVertexArray(0);

    /* 纹理处理阶段 */
    grass = std::make_unique<core::Texture>("mix/grass.jpg", 0);
    land = std::make_unique<core::Texture>("mix/land.jpg", 1);
    noise = std::make_unique<core::Texture>("mix/noise.jpg", 2);
}

void render(GLuint vao)
{
    // render阶段
    glClear(GL_COLOR_BUFFER_BIT);
    shader->Begin();

    // 设置uniform变量
    shader->SetInt("grassSampler", 0);
    shader->SetInt("landSampler", 1);
    shader->SetInt("noiseSampler", 2);
    shader->SetMat4("transform", camera->GetProjectionMatrix() * camera->GetViewMatrix());

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (void *)0);
    glBindVertexArray(0);
    shader->End();
}

int main()
{
    core::Logger::Init();
    /* ************************************************************************************************ */
    auto &App = core::Application::GetInstance();
    constexpr uint32_t WIDTH = 1960;
    constexpr uint32_t HEIGHT = 1080;

    if (!App.Init(WIDTH, HEIGHT))
        return -1;

    camera = std::make_shared<core::Camera>(core::Camera::CreatePerspective(45.f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.f));
    // camera = std::make_shared<core::Camera>(core::Camera::CreateOrthographic(-1.f, 1.f, 1.f, -1.f, 0.1f, 100.f));
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

    App.SetMouseCallback([&App](GLFWwindow *window, int button, int action, int mods) { // 鼠标点击监听
        // TODO 鼠标点击事件
        GL_TRACE("Mouse Click");
    });

    App.SetCursorCallback([](GLFWwindow *window, double xpos, double ypos) { // 鼠标位置监听
        controller->OnCursorPos(window, xpos, ypos);
    });

    App.SetScrollCallback([](GLFWwindow *window, double xoffset, double yoffset) { // 鼠标滚轮监听
        controller->OnScroll(window, xoffset, yoffset);
    });

    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0.2f, 0.3f, 0.3f, 1.f);

    GLuint vao = 0;
    Prepare(vao);

    while (App.Update())
    {
        controller->Update(App.GetDeltaTime());

        render(vao);
    }

    App.Destroy();
    /* ************************************************************************************************ */
    core::Logger::Shutdown();
    return 0;
}

/*
（重要前提：我目前在实现一个opengl的个人渲染器用来作为简历找工作的项目，不需要像商业级引擎一样复杂，只需要项目结构清晰明了，实现高效且优雅，符合工程实践。）
*/