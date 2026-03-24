#include "mesh.hpp"
#include "utils/logger.hpp"
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cstddef>
#include <utility>

namespace core
{
    void Mesh::Release()
    {
        if (mEBO != 0)
        {
            glDeleteBuffers(1, &mEBO);
            mEBO = 0;
        }

        if (mVBO != 0)
        {
            glDeleteBuffers(1, &mVBO);
            mVBO = 0;
        }

        if (mVAO != 0)
        {
            glDeleteVertexArrays(1, &mVAO);
            mVAO = 0;
        }

        mVertexCount = 0;
        mIndexCount = 0;
    }

    Mesh::~Mesh()
    {
        Release();
    }

    Mesh::Mesh(Mesh &&other) noexcept
    {
        *this = std::move(other);
    }

    Mesh &Mesh::operator=(Mesh &&other) noexcept
    {
        // 避免自赋值
        if (this == &other)
            return *this;

        Release(); // 释放当前资源

        mVAO = other.mVAO;
        mVBO = other.mVBO;
        mEBO = other.mEBO;
        mVertexCount = other.mVertexCount;
        mIndexCount = other.mIndexCount;

        // 重置other状态, 确保不会误用已移动的资源
        other.mVAO = 0;
        other.mVBO = 0;
        other.mEBO = 0;
        other.mVertexCount = 0;
        other.mIndexCount = 0;

        return *this;
    }

    void Mesh::SetData(const std::vector<MeshVertex> &vertices, const std::vector<uint32_t> &indices)
    {
        if (vertices.empty())
        {
            GL_CRITICAL("[Mesh] SetData Failed: Vertices Array Is Empty");
            return;
        }

        mVertexCount = static_cast<uint32_t>(vertices.size());
        mIndexCount = static_cast<uint32_t>(indices.size());

        // 懒创建VAO, VBO, EBO
        if (mVAO == 0)
            glGenVertexArrays(1, &mVAO);
        if (mVBO == 0)
            glGenBuffers(1, &mVBO);

        glBindVertexArray(mVAO);

        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)), vertices.data(), GL_STATIC_DRAW);

        if (!indices.empty())
        {
            if (mEBO == 0)
                glGenBuffers(1, &mEBO);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);
        }
        else
        {
            // 非索引网格: 解绑并释放旧EBO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            if (mEBO != 0)
            {
                glDeleteBuffers(1, &mEBO);
                mEBO = 0;
            }
        }

        // location 0 -> position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void *>(offsetof(MeshVertex, __position__)));

        // location 1 -> uv
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void *>(offsetof(MeshVertex, __uv__)));

        // location 2 -> normal
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void *>(offsetof(MeshVertex, __normal__)));

        // location 3 -> tangent
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), reinterpret_cast<void *>(offsetof(MeshVertex, __tangent__)));

        glBindVertexArray(0);
    }

    std::shared_ptr<Mesh> Mesh::CreatePlane(float size)
    {
        const float h = size * 0.5f;

        std::vector<MeshVertex> vertices = {
            {{-h, -h, 0.f}, {0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},
            {{h, -h, 0.f}, {1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},
            {{h, h, 0.f}, {1.f, 1.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},
            {{-h, h, 0.f}, {0.f, 1.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,
            2, 3, 0};

        auto mesh = std::make_shared<Mesh>();
        mesh->SetData(vertices, indices);
        return mesh;
    }

    std::shared_ptr<Mesh> Mesh::CreateCube(float size)
    {
        const float h = size * 0.5f;

        std::vector<MeshVertex> vertices = {
            // +Z
            {{-h, -h, h}, {0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},
            {{h, -h, h}, {1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},
            {{h, h, h}, {1.f, 1.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},
            {{-h, h, h}, {0.f, 1.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}},

            // -Z
            {{h, -h, -h}, {0.f, 0.f}, {0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f}},
            {{-h, -h, -h}, {1.f, 0.f}, {0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f}},
            {{-h, h, -h}, {1.f, 1.f}, {0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f}},
            {{h, h, -h}, {0.f, 1.f}, {0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f}},

            // +Y
            {{-h, h, h}, {0.f, 0.f}, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}},
            {{h, h, h}, {1.f, 0.f}, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}},
            {{h, h, -h}, {1.f, 1.f}, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}},
            {{-h, h, -h}, {0.f, 1.f}, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}},

            // -Y
            {{-h, -h, -h}, {0.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}},
            {{h, -h, -h}, {1.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}},
            {{h, -h, h}, {1.f, 1.f}, {0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}},
            {{-h, -h, h}, {0.f, 1.f}, {0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}},

            // +X
            {{h, -h, h}, {0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}},
            {{h, -h, -h}, {1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}},
            {{h, h, -h}, {1.f, 1.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}},
            {{h, h, h}, {0.f, 1.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}},
            // -X
            {{-h, -h, -h}, {0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
            {{-h, -h, h}, {1.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
            {{-h, h, h}, {1.f, 1.f}, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
            {{-h, h, -h}, {0.f, 1.f}, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
        };

        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4,
            8, 9, 10, 10, 11, 8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20};

        auto mesh = std::make_shared<Mesh>();
        mesh->SetData(vertices, indices);
        return mesh;
    }

    std::shared_ptr<Mesh> Mesh::CreateSphere(float radius, uint32_t latitude, uint32_t longitude)
    {
        latitude = std::max<uint32_t>(latitude, 3u);
        longitude = std::max<uint32_t>(longitude, 3u);

        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve((latitude + 1) * (longitude + 1));
        indices.reserve(latitude * longitude * 6);

        for (uint32_t i = 0; i <= latitude; ++i)
        {
            const float v = static_cast<float>(i) / static_cast<float>(latitude);
            const float phi = glm::pi<float>() * v;
            const float sin_phi = std::sinf(phi);
            const float cos_phi = std::cosf(phi);

            for (uint32_t j = 0; j <= longitude; ++j)
            {
                const float u = static_cast<float>(j) / static_cast<float>(longitude); 
                const float theta = glm::two_pi<float>() * u;

                const float x = radius * sin_phi * std::cosf(theta);
                const float y = radius * cos_phi;
                const float z = radius * sin_phi * std::sinf(theta);

                glm::vec3 pos{x, y, z};
                glm::vec3 n = glm::normalize(pos);

                // 切线沿经度方向(θ的偏导): (sinθ, 0, -cosθ) × 法线垂直分量
                // 切线tangent = (sinθ, 0, -cosθ) 沿经度方向, 与法线垂直
                glm::vec3 tangent = glm::normalize(glm::vec3(std::sinf(theta), 0.0f, -std::cosf(theta)));

                // 处理极点特殊情况(phi=0 或 phi=π时, sin_phi=0, 切线可能失效)
                if (sin_phi < 1e-6)
                    tangent = glm::vec3(1.f, 0.f, 0.f); // 极点处切线沿X轴, 副切线沿Z轴(避免零向量)

                vertices.push_back({pos, {1.f - u, 1.f - v}, n, tangent});
            }
        }

        for (uint32_t i = 0; i < latitude; ++i)
        {
            for (uint32_t j = 0; j < longitude; ++j)
            {
                const uint32_t p1 = i * (longitude + 1) + j;
                const uint32_t p2 = p1 + longitude + 1;
                const uint32_t p3 = p1 + 1;
                const uint32_t p4 = p2 + 1;

                indices.push_back(p1);
                indices.push_back(p2);
                indices.push_back(p3);

                indices.push_back(p3);
                indices.push_back(p2);
                indices.push_back(p4);
            }
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->SetData(vertices, indices);
        return mesh;
    }
}