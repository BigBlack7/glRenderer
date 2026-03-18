#include "loader.hpp"
#include "model.hpp"
#include "geometry/mesh.hpp"
#include "material/material.hpp"
#include "shader/shader.hpp"
#include "texture/texture.hpp"
#include "utils/logger.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core
{
    namespace detail
    {
        const std::filesystem::path ModelRoot = "../../../assets/model/";

        std::filesystem::path ResolveModelPath(const std::filesystem::path &modelPath)
        {
            if (modelPath.empty()) // 路径为空
                return {};

            if (modelPath.is_absolute() && std::filesystem::exists(modelPath)) // 绝对路径存在
                return modelPath;

            if (std::filesystem::exists(modelPath)) // 相对路径存在
                return modelPath;

            const std::filesystem::path inAssets = ModelRoot / modelPath; // 在资源目录中查找
            if (std::filesystem::exists(inAssets))
                return inAssets;

            return modelPath;
        }

        // Assimp矩阵(行主序)转换为GLM矩阵(列主序)
        glm::mat4 ToMat4(const aiMatrix4x4 &m)
        {
            glm::mat4 out(1.f);
            out[0][0] = m.a1;
            out[1][0] = m.a2;
            out[2][0] = m.a3;
            out[3][0] = m.a4;
            out[0][1] = m.b1;
            out[1][1] = m.b2;
            out[2][1] = m.b3;
            out[3][1] = m.b4;
            out[0][2] = m.c1;
            out[1][2] = m.c2;
            out[2][2] = m.c3;
            out[3][2] = m.c4;
            out[0][3] = m.d1;
            out[1][3] = m.d2;
            out[2][3] = m.d3;
            out[3][3] = m.d4;
            return out;
        }

        // 将Assimp矩阵转换为Transform
        Transform ToTransform(const aiMatrix4x4 &m)
        {
            Transform t{};
            const glm::mat4 mat = ToMat4(m);

            glm::vec3 scale(1.f);                   // 缩放向量
            glm::quat rotation(1.f, 0.f, 0.f, 0.f); // 旋转四元数
            glm::vec3 translation(0.f);             // 平移向量
            glm::vec3 skew(0.f);                    // 倾斜向量
            glm::vec4 perspective(0.f);             // 透视向量

            if (glm::decompose(mat, scale, rotation, translation, skew, perspective)) // 矩阵分解
            {
                t.SetPosition(translation);  // 设置平移
                t.SetRotationQuat(rotation); // 设置旋转
                t.SetScale(scale);           // 设置缩放
            }

            return t;
        }

        // 从Assimp网格创建Mesh
        std::shared_ptr<Mesh> CreateMesh(const aiMesh &sourceMesh)
        {
            // 预分配顶点
            std::vector<MeshVertex> vertices{};
            vertices.resize(sourceMesh.mNumVertices);

            for (uint32_t i = 0; i < sourceMesh.mNumVertices; ++i)
            {
                MeshVertex v{};                                                                                              // 创建顶点
                v.__position__ = glm::vec3(sourceMesh.mVertices[i].x, sourceMesh.mVertices[i].y, sourceMesh.mVertices[i].z); // 设置顶点位置

                if (sourceMesh.HasNormals()) // 设置法线
                {
                    v.__normal__ = glm::vec3(sourceMesh.mNormals[i].x, sourceMesh.mNormals[i].y, sourceMesh.mNormals[i].z);
                }

                if (sourceMesh.HasTextureCoords(0)) // 设置纹理UV坐标
                {
                    v.__uv__ = glm::vec2(sourceMesh.mTextureCoords[0][i].x, sourceMesh.mTextureCoords[0][i].y);
                }

                vertices[i] = v;
            }

            // 预分配索引
            std::vector<uint32_t> indices{};
            indices.reserve(static_cast<size_t>(sourceMesh.mNumFaces) * 3u);
            for (uint32_t i = 0; i < sourceMesh.mNumFaces; ++i) // 遍历所有面
            {
                const aiFace &face = sourceMesh.mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; ++j) // 遍历面的所有索引
                    indices.push_back(face.mIndices[j]);
            }

            auto mesh = std::make_shared<Mesh>();
            mesh->SetData(vertices, indices);
            return mesh;
        }

        // 注册网格骨骼信息到模型
        void RegisterMeshBones(const aiMesh &sourceMesh, Model &model)
        {
            for (uint32_t i = 0; i < sourceMesh.mNumBones; ++i) // 遍历所有骨骼
            {
                const aiBone *bone = sourceMesh.mBones[i];
                if (!bone)
                    continue;

                model.RegisterBone(bone->mName.C_Str(), ToMat4(bone->mOffsetMatrix)); // 注册骨骼
            }
        }

        // 检查材质是否包含任何指定类型的纹理
        bool HasAnyTexture(const aiMaterial &aiMaterial, std::initializer_list<aiTextureType> types)
        {
            // 遍历所有纹理类型
            for (const aiTextureType type : types)
            {
                if (aiMaterial.GetTextureCount(type) > 0u) // 如果该类型纹理数量大于0返回true
                    return true;
            }
            return false; // 都没找到返回false
        }

        // 加载嵌入式纹理
        std::shared_ptr<Texture> LoadEmbeddedTexture(const aiTexture &embedded, std::string_view debugName, bool flipY)
        {
            Texture::CreateInfo info{};
            info.__flipY__ = flipY;

            if (embedded.mHeight == 0) // 如果是压缩的纹理数据(例如png, jpg高度为0)
            {
                const auto *bytes = reinterpret_cast<const uint8_t *>(embedded.pcData);   // 获取数据指针
                const std::span<const uint8_t> blob(bytes, embedded.mWidth);              // 创建数据span
                return std::make_shared<Texture>(blob, 0u, std::string(debugName), info); // 创建纹理
            }

            // 处理未压缩的纹理数据
            const size_t texelCount = static_cast<size_t>(embedded.mWidth) * static_cast<size_t>(embedded.mHeight);
            std::vector<uint8_t> rgba(texelCount * 4u, 255u); // RGBA数据初始化为255

            // 遍历所有纹素
            for (size_t i = 0; i < texelCount; ++i)
            {
                const aiTexel &src = embedded.pcData[i]; // 获取源纹素
                rgba[i * 4u + 0u] = src.r;
                rgba[i * 4u + 1u] = src.g;
                rgba[i * 4u + 2u] = src.b;
                rgba[i * 4u + 3u] = src.a;
            }
            return std::make_shared<Texture>(rgba.data(), embedded.mWidth, embedded.mHeight, 0u, std::string(debugName), info);
        }

        // 带缓存纹理加载
        std::shared_ptr<Texture> LoadTextureWithCache(const aiMaterial &aiMaterial,
                                                      const aiScene &aiSceneRef,
                                                      const std::filesystem::path &modelDir,
                                                      std::initializer_list<aiTextureType> candidates,
                                                      bool flipY,
                                                      std::unordered_map<std::string, std::shared_ptr<Texture>> &textureCache)
        {
            for (const aiTextureType type : candidates) // 遍历候选纹理类型
            {
                if (aiMaterial.GetTextureCount(type) == 0u) // 没有该类型纹理跳过
                    continue;

                aiString texPath{};                                         // 纹理路径
                if (aiMaterial.GetTexture(type, 0, &texPath) != AI_SUCCESS) // 获取纹理路径失败跳过
                    continue;

                const std::string rawPath = texPath.C_Str(); // 获取路径字符串
                if (rawPath.empty())                         // 如果路径为空跳过
                    continue;

                // 1) 检查是否是嵌入式纹理
                if (const aiTexture *embedded = aiSceneRef.GetEmbeddedTexture(rawPath.c_str()))
                {
                    const std::string cacheKey = "embedded:" + rawPath;
                    if (const auto it = textureCache.find(cacheKey); it != textureCache.end()) // 如果缓存中存在则直接返回该纹理
                        return it->second;

                    auto tex = LoadEmbeddedTexture(*embedded, rawPath, flipY); // 加载嵌入式纹理
                    // 如果加载成功且有效
                    if (tex && tex->IsValid())
                    {
                        textureCache.emplace(cacheKey, tex); // 添加到缓存后返回纹理
                        return tex;
                    }

                    GL_WARN("[ModelLoader] Embedded Texture Load Failed: {}", rawPath);
                    continue; // 继续尝试其他类型
                }

                // 2) 处理外部纹理文件
                const std::filesystem::path texturePath(rawPath);                                                           // 纹理路径
                const std::filesystem::path cachePath = texturePath.is_absolute() ? texturePath : (modelDir / texturePath); // 缓存路径
                const std::string cacheKey = cachePath.lexically_normal().generic_string();                                 // 标准化缓存键

                if (const auto it = textureCache.find(cacheKey); it != textureCache.end()) // 如果缓存中存在则直接返回该纹理
                    return it->second;

                Texture::CreateInfo info{};                                            // 纹理创建信息
                info.__flipY__ = flipY;                                                // 是否翻转Y轴
                auto tex = std::make_shared<Texture>(texturePath, 0u, modelDir, info); // 加载纹理
                // 如果加载成功且有效
                if (tex && tex->IsValid())
                {
                    textureCache.emplace(cacheKey, tex); // 添加到缓存后返回纹理
                    return tex;
                }

                GL_WARN("[ModelLoader] External Texture Load Failed: {}", rawPath);
            }
            return nullptr;
        }

        std::shared_ptr<Texture> LoadTexturePathWithCache(const std::filesystem::path &texturePath,
                                                          const std::filesystem::path &modelDir,
                                                          bool flipY,
                                                          std::unordered_map<std::string, std::shared_ptr<Texture>> &textureCache)
        {
            if (texturePath.empty())
                return nullptr;

            const std::filesystem::path cachePath = texturePath.is_absolute() ? texturePath : (modelDir / texturePath);
            const std::string cacheKey = cachePath.lexically_normal().generic_string();

            if (const auto it = textureCache.find(cacheKey); it != textureCache.end())
                return it->second;

            Texture::CreateInfo info{};
            info.__flipY__ = flipY;
            auto tex = std::make_shared<Texture>(texturePath, 0u, modelDir, info);
            if (tex && tex->IsValid())
            {
                textureCache.emplace(cacheKey, tex);
                return tex;
            }

            GL_WARN("[ModelLoader] Manual Texture Load Failed: {}", texturePath.string());
            return nullptr;
        }

        // 从Assimp材质创建Material对象
        std::shared_ptr<Material> CreateMaterial(const aiMaterial &aiMaterial,
                                                 const aiScene &aiSceneRef,
                                                 const std::filesystem::path &modelDir,
                                                 const ModelLoadOptions &options,
                                                 std::unordered_map<std::string, std::shared_ptr<Texture>> &textureCache,
                                                 const std::string &modelName)
        {
            auto material = std::make_shared<Material>(options.__shader__); // 创建材质对象

            // 日志标识当前处理的模型
            static thread_local int materialCounter = 0;
            materialCounter++;
            GL_INFO("[ModelLoader] Processing Material #{} For Model: '{}'", materialCounter, modelName);

            aiColor4D color{}; // 颜色
            // 尝试获取基础颜色或漫反射颜色
            if (AI_SUCCESS == aiGetMaterialColor(&aiMaterial, AI_MATKEY_BASE_COLOR, &color) || AI_SUCCESS == aiGetMaterialColor(&aiMaterial, AI_MATKEY_COLOR_DIFFUSE, &color))
                material->SetVec3("uDefaultColor", glm::vec3(color.r, color.g, color.b)); // 设置材质颜色
            else
                material->SetVec3("uDefaultColor", glm::vec3(1.f, 1.f, 1.f)); // 默认白模

            ai_real shininess = 64.f; // 光泽度
            // 尝试获取光泽度
            if (AI_SUCCESS == aiGetMaterialFloat(&aiMaterial, AI_MATKEY_SHININESS, &shininess))
                material->SetFloat("uShininess", std::max(1.f, static_cast<float>(shininess))); // 设置光泽度最小为1
            else
                material->SetFloat("uShininess", 64.f); // 默认光泽度64

            // 尝试获取透明度
            ai_real opacityRaw = 1.f; // 原始透明度
            if (AI_SUCCESS == aiGetMaterialFloat(&aiMaterial, AI_MATKEY_OPACITY, &opacityRaw))
                opacityRaw = std::clamp(opacityRaw, static_cast<ai_real>(0.f), static_cast<ai_real>(1.f));
            const float opacity = static_cast<float>(opacityRaw);

            // 加载反照率纹理(Albedo/Diffuse)
            if (auto albedo = LoadTextureWithCache(
                    aiMaterial,
                    aiSceneRef,
                    modelDir,
                    {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE},
                    options.__flipTextureY__,
                    textureCache))
            {
                GL_INFO("[ModelLoader] '{}' - Albedo Texture Loaded Successfully", modelName);
                material->SetTexture(TextureSlot::Albedo, std::move(albedo));
            }

            // 加载金属度/粗糙度纹理(Blinn-Phong中视为SpecularMask)
            if (auto metallicRough = LoadTextureWithCache(
                    aiMaterial,
                    aiSceneRef,
                    modelDir,
                    {aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SPECULAR, aiTextureType_SHININESS},
                    options.__flipTextureY__,
                    textureCache))
            {
                GL_INFO("[ModelLoader] '{}' - MetallicRoughness Texture Loaded Successfully", modelName);
                material->SetTexture(TextureSlot::MetallicRoughness, std::move(metallicRough));
            }

            // TODO: Normal map通道接入TBN后开启
            if (HasAnyTexture(aiMaterial, {aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA, aiTextureType_HEIGHT}))
                GL_DEBUG("[ModelLoader] TODO: '{}' - Normal Map Detected But Not Consumed Yet", modelName);

            // TODO: AO通道接入后开启
            if (HasAnyTexture(aiMaterial, {aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP}))
                GL_DEBUG("[ModelLoader] TODO: '{}' - AO Map Detected But Not Consumed Yet", modelName);

            // TODO: Emissive通道接入后开启
            if (HasAnyTexture(aiMaterial, {aiTextureType_EMISSIVE, aiTextureType_EMISSION_COLOR}))
                GL_DEBUG("[ModelLoader] TODO: '{}' - Emissive Map Detected But Not Consumed Yet", modelName);

            if (auto opacityMask = LoadTextureWithCache(
                    aiMaterial,
                    aiSceneRef,
                    modelDir,
                    {aiTextureType_OPACITY},
                    options.__flipTextureY__,
                    textureCache))
            {
                GL_INFO("[ModelLoader] '{}' - Opacity Mask Texture Loaded Successfully", modelName);
                material->SetTexture(TextureSlot::OpacityMask, std::move(opacityMask));
            }

            for (const auto &[slot, texturePath] : options.__globalTextureOverrides__)
            {
                const auto existingTexture = material->GetTexture(slot);
                if (existingTexture && !options.__allowOverrideLoadedTextures__)
                {
                    GL_WARN("[ModelLoader] '{}' - Global Texture Fallback Skipped: Slot={}, Existing='{}'", modelName, static_cast<uint32_t>(slot), existingTexture->GetDebugName());
                    continue;
                }

                if (auto manualTexture = LoadTexturePathWithCache(texturePath, modelDir, options.__flipTextureY__, textureCache))
                {
                    if (existingTexture && options.__allowOverrideLoadedTextures__)
                    {
                        GL_WARN("[ModelLoader] '{}' - Global Texture Fallback Replaced Existing Texture: Slot={}, Old='{}', New='{}'", modelName, static_cast<uint32_t>(slot), existingTexture->GetDebugName(), manualTexture->GetDebugName());
                    }
                    else
                    {
                        GL_INFO("[ModelLoader] '{}' - Global Texture Fallback Applied: Slot={}, Path='{}'", modelName, static_cast<uint32_t>(slot), texturePath.generic_string());
                    }

                    material->SetTexture(slot, manualTexture);
                }
            }

            // 根据透明度判断渲染状态
            RenderStateDesc renderState = MakeOpaqueState();
            renderState.mOpacity = opacity;
            const bool hasOpacityMask = (material->GetTexture(TextureSlot::OpacityMask) != nullptr);
            if (opacity < 0.999f)
            {
                renderState = MakeTransparentState();
                renderState.mOpacity = opacity;
            }
            else if (hasOpacityMask)
            {
                renderState = MakeCutoutState(0.5f);
                renderState.mOpacity = opacity;
            }
            material->SetRenderState(renderState);

            return material;
        }

        /*  分离变换与渲染数据
            Assimp结构:
            aiNode "Car"
            {
                transformation: [位置、旋转、缩放]
                meshes: [0, 1, 2, 3] -> 4个网格共享同一个变换
            }

            引擎内部结构:
            TransformNode "Car" (只有变换, 没有渲染数据)
            ├── MeshNode "Car_Mesh0" (车身 + 金属材质)
            ├── MeshNode "Car_Mesh1" (车轮 + 橡胶材质)
            ├── MeshNode "Car_Mesh2" (车灯 + 发光材质)
            └── MeshNode "Car_Mesh3" (内饰 + 布料材质)
        */
        ModelNodeID BuildNodeRecursive(const aiNode &aiNodeRef,
                                       Model &model,
                                       ModelNodeID parentNode,
                                       const aiScene &aiSceneRef,
                                       const std::vector<std::shared_ptr<Mesh>> &meshes,
                                       const std::vector<std::shared_ptr<Material>> &materials,
                                       const std::shared_ptr<Material> &fallbackMaterial)
        {
            ModelNode transformNode{};                                                 // 创建变换节点
            transformNode.__name__ = aiNodeRef.mName.C_Str();                          // 设置节点名称(Assimp节点名)
            transformNode.__localTransform__ = ToTransform(aiNodeRef.mTransformation); // 设置本地变换

            const ModelNodeID transformNodeID = model.AddNode(std::move(transformNode)); // 添加节点到模型
            // 如果有父节点, 建立父子关系
            if (parentNode != InvalidModelNodeID)
                model.AddChild(parentNode, transformNodeID);

            // 处理节点关联的网格
            for (uint32_t i = 0; i < aiNodeRef.mNumMeshes; ++i)
            {
                const uint32_t meshIndex = aiNodeRef.mMeshes[i];                      // 网格索引
                if (meshIndex >= meshes.size() || meshIndex >= aiSceneRef.mNumMeshes) // 索引检查跳过无效索引
                    continue;

                ModelNode meshNode{};                                                                   // 创建网格节点
                meshNode.__name__ = std::string(aiNodeRef.mName.C_Str()) + "_Mesh" + std::to_string(i); // 设置节点名称(Assimp节点名 + 网格索引)
                meshNode.__mesh__ = meshes[meshIndex];                                                  // 设置网格
                meshNode.__material__ = fallbackMaterial;                                               // 默认使用回退材质

                const aiMesh *meshRef = aiSceneRef.mMeshes[meshIndex];                                           // 获取Assimp网格引用
                if (meshRef && meshRef->mMaterialIndex < materials.size() && materials[meshRef->mMaterialIndex]) // 检查材质有效性
                    meshNode.__material__ = materials[meshRef->mMaterialIndex];                                  // 使用实际材质

                const ModelNodeID meshNodeID = model.AddNode(std::move(meshNode)); // 添加网格节点到模型
                model.AddChild(transformNodeID, meshNodeID);                       // 将网格节点添加为变换节点的子节点
            }

            // 递归处理子节点
            for (uint32_t i = 0; i < aiNodeRef.mNumChildren; ++i)
            {
                const aiNode *child = aiNodeRef.mChildren[i]; // 获取子节点
                if (!child)                                   // 跳过空子节点
                    continue;

                BuildNodeRecursive(*child, model, transformNodeID, aiSceneRef, meshes, materials, fallbackMaterial);
            }
            return transformNodeID;
        }
    }

    std::shared_ptr<Model> ModelLoader::Load(const std::filesystem::path &modelPath, const ModelLoadOptions &options)
    {
        const std::filesystem::path resolvedPath = detail::ResolveModelPath(modelPath); // 解析路径
        if (resolvedPath.empty())
        {
            GL_ERROR("[ModelLoader] Load Empty Model Path: {}", resolvedPath.string());
            return nullptr;
        }

        Assimp::Importer importer{};
        // 合并相同顶点 | 改进缓存局部性 | 按图元类型排序
        uint32_t flags = aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_SortByPType;
        if (options.__triangulate__) // 如果需要三角化, 则添加三角化标志
            flags |= aiProcess_Triangulate;
        if (options.__genSmoothNormals__) // 如果需要生成平滑法线, 则添加平滑法线标志
            flags |= aiProcess_GenSmoothNormals;

        const aiScene *aiSceneRef = importer.ReadFile(resolvedPath.string(), flags);                   // 以指定标志读取模型文件
        if (!aiSceneRef || !aiSceneRef->mRootNode || (aiSceneRef->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) // 检查加载结果
        {
            GL_ERROR("[ModelLoader] Assimp Import Failed: {} | {}", resolvedPath.string(), importer.GetErrorString());
            return nullptr;
        }

        // 创建模型对象, 使用文件名作为名称
        auto model = std::make_shared<Model>(resolvedPath.stem().string());

        // 处理所有网格
        std::vector<std::shared_ptr<Mesh>> meshes(aiSceneRef->mNumMeshes); // 预分配网格数组
        for (uint32_t i = 0; i < aiSceneRef->mNumMeshes; ++i)
        {
            const aiMesh *meshRef = aiSceneRef->mMeshes[i];
            if (!meshRef) // 跳过空网格
                continue;

            meshes[i] = detail::CreateMesh(*meshRef);    // 创建网格对象
            detail::RegisterMeshBones(*meshRef, *model); // 注册网格骨骼
        }

        // 处理所有材质
        std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache{};    // 纹理缓存
        std::vector<std::shared_ptr<Material>> materials(aiSceneRef->mNumMaterials); // 预分配材质数组
        for (uint32_t i = 0; i < aiSceneRef->mNumMaterials; ++i)
        {
            const aiMaterial *matRef = aiSceneRef->mMaterials[i];
            if (!matRef) // 跳过空材质
                continue;

            materials[i] = detail::CreateMaterial(*matRef, *aiSceneRef, resolvedPath.parent_path(), options, textureCache, model->GetName());
        }

        // 创建默认材质
        auto fallbackMaterial = std::make_shared<Material>(options.__shader__);
        fallbackMaterial->SetVec3("uDefaultColor", glm::vec3(1.f, 1.f, 1.f));
        fallbackMaterial->SetFloat("uShininess", 64.f);

        detail::BuildNodeRecursive(*aiSceneRef->mRootNode, *model, InvalidModelNodeID, *aiSceneRef, meshes, materials, fallbackMaterial);

        model->ResolveBoneNodes(); // 解析骨骼节点关联

        GL_INFO("[ModelLoader] Loaded Model: {} | Nodes: {} | Meshes: {} | Materials: {} | Bones: {}",
                resolvedPath.string(), model->GetNodes().size(), aiSceneRef->mNumMeshes, aiSceneRef->mNumMaterials, model->GetBones().size());

        return model;
    }
}