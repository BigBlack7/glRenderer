// core
#include <utils/logger.hpp>
#include <application/application.hpp>
#include <shader/shader.hpp>

std::unique_ptr<core::Shader> shader = nullptr;
void Prepare(GLuint &vao)
{
    /* shader处理阶段 */
    shader = std::make_unique<core::Shader>("primary/triangle.vert", "primary/triangle.frag");

    /* 数据处理阶段 */
    float postions[] = {
        -0.5f, -0.5f, 0.f, // left
        0.5f, -0.5f, 0.f,  // right
        0.f, 0.5f, 0.f,    // top
        0.5f, 0.5f, 0.f,   // top-right
        0.8f, 0.8f, 0.f,   // left-top
        0.8f, 0.f, 0.f     // right-bottom
    };

    float colors[] = {
        1.f, 0.f, 0.f,    // red
        0.f, 1.f, 0.f,    // green
        0.f, 0.f, 1.f,    // blue
        1.f, 1.f, 0.f,    // yellow
        0.4f, 0.4f, 0.8f, // gray
        0.5f, 0.5f, 0.5f  // white
    };

    uint8_t indices[] = {
        0, 1, 2, // first triangle
        2, 1, 3  // second triangle
    };

    // 顶点缓冲对象(VBO)和元素缓冲对象(EBO)
    GLuint posVBO = 0;
    glGenBuffers(1, &posVBO);
    glBindBuffer(GL_ARRAY_BUFFER, posVBO);                                     // 绑定顶点缓冲对象, 后续的操作都将对该缓冲对象进行操作
    glBufferData(GL_ARRAY_BUFFER, sizeof(postions), postions, GL_STATIC_DRAW); // 开辟显存, 传输数据到显存中

    GLuint colorVBO = 0;
    glGenBuffers(1, &colorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);

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

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindVertexArray(0);
}

void render(GLuint vao)
{
    // render阶段
    glClear(GL_COLOR_BUFFER_BIT);
    shader->Begin();

    shader->SetFloat("time", (float)glfwGetTime());
    float icolor[] = {0.3f, 0.4f, 0.5f};
    shader->SetVec3("icolor", std::move(icolor));
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, (void *)0);
    glBindVertexArray(0);
    shader->End();
}

int main()
{
    core::Logger::Init();
    /* ************************************************** */
    auto &App = core::Application::GetInstance();
    constexpr uint32_t WIDTH = 1960;
    constexpr uint32_t HEIGHT = 1080;

    if (!App.Init(WIDTH, HEIGHT))
        return -1;

    // 注册各类回调函数
    App.SetResizeCallback([](uint32_t width, uint32_t height) { // 窗口大小监听
        GL_TRACE("OnResize: width={}, height={}", width, height);
        glViewport(0, 0, width, height);
    });

    App.SetKeyBoardCallback([](GLFWwindow *window, int key, int scancode, int action, int mods) { // 键盘监听
        GL_TRACE("Click {}", key);
    });

    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0.2f, 0.3f, 0.3f, 1.f);

    GLuint vao = 0;
    Prepare(vao);

    while (App.Update())
    {
        render(vao);
    }

    App.Destroy();
    /* ************************************************** */
    core::Logger::Shutdown();
    return 0;
}