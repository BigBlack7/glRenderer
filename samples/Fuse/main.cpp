#include <utils/logger.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <vector>
#include <string>
#include <cmath>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// 获取文件夹中的所有图片文件(按文件名排序)
std::vector<std::string> GetImageFiles(const std::string &folderPath)
{
    std::vector<std::string> imageFiles;

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath))
    {
        GL_ERROR("文件夹不存在或不是目录: {}", folderPath);
        return imageFiles;
    }

    // 支持的图片格式
    std::vector<std::string> validExtensions = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};

    for (const auto &entry : fs::directory_iterator(folderPath))
    {
        if (entry.is_regular_file())
        {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

            if (std::find(validExtensions.begin(), validExtensions.end(), extension) != validExtensions.end())
            {
                imageFiles.push_back(entry.path().string());
            }
        }
    }

    // 按文件名排序
    std::sort(imageFiles.begin(), imageFiles.end());

    return imageFiles;
}

// 图像混合类
class ImageBlender
{
private:
    std::vector<unsigned char> image1_data;
    std::vector<unsigned char> image2_data;
    std::vector<unsigned char> result_data;

    int width1, height1, channels1;
    int width2, height2, channels2;
    int result_width, result_height, result_channels;

public:
    ImageBlender() : width1(0), height1(0), channels1(0),
                     width2(0), height2(0), channels2(0),
                     result_width(0), result_height(0), result_channels(0) {}

    // 加载第一张图片
    bool LoadImage1(const std::string &filename)
    {
        unsigned char *data = stbi_load(filename.c_str(), &width1, &height1, &channels1, 0);
        if (!data)
        {
            GL_ERROR("无法加载图片1: {}", filename);
            return false;
        }

        // 确保图片有4个通道(RGBA)
        if (channels1 < 3)
        {
            GL_ERROR("图片1必须有至少3个通道(RGB)");
            stbi_image_free(data);
            return false;
        }

        // 转换为RGBA格式
        if (channels1 == 3)
        {
            image1_data.resize(width1 * height1 * 4);
            for (int i = 0; i < width1 * height1; i++)
            {
                image1_data[i * 4 + 0] = data[i * 3 + 0]; // R
                image1_data[i * 4 + 1] = data[i * 3 + 1]; // G
                image1_data[i * 4 + 2] = data[i * 3 + 2]; // B
                image1_data[i * 4 + 3] = 255;             // A
            }
            channels1 = 4;
        }
        else
        {
            image1_data.assign(data, data + width1 * height1 * channels1);
        }

        stbi_image_free(data);
        GL_INFO("成功加载图片1: {} ({}x{}x{})", filename, width1, height1, channels1);
        return true;
    }

    // 加载第二张图片
    bool LoadImage2(const std::string &filename)
    {
        unsigned char *data = stbi_load(filename.c_str(), &width2, &height2, &channels2, 0);
        if (!data)
        {
            GL_ERROR("无法加载图片2: {}", filename);
            return false;
        }

        // 确保图片有4个通道(RGBA)
        if (channels2 < 3)
        {
            GL_ERROR("图片2必须有至少3个通道(RGB)");
            stbi_image_free(data);
            return false;
        }

        // 转换为RGBA格式
        if (channels2 == 3)
        {
            image2_data.resize(width2 * height2 * 4);
            for (int i = 0; i < width2 * height2; i++)
            {
                image2_data[i * 4 + 0] = data[i * 3 + 0]; // R
                image2_data[i * 4 + 1] = data[i * 3 + 1]; // G
                image2_data[i * 4 + 2] = data[i * 3 + 2]; // B
                image2_data[i * 4 + 3] = 255;             // A
            }
            channels2 = 4;
        }
        else
        {
            image2_data.assign(data, data + width2 * height2 * channels2);
        }

        stbi_image_free(data);
        GL_INFO("成功加载图片2: {} ({}x{}x{})", filename, width2, height2, channels2);
        return true;
    }

    // 混合两张图片
    bool BlendImages(float alpha)
    {
        // 确保 alpha 在合理范围内
        alpha = std::max(0.0f, std::min(1.0f, alpha));

        // 使用第一张图片的尺寸作为结果尺寸
        result_width = width1;
        result_height = height1;
        result_channels = 4; // RGBA
        result_data.resize(result_width * result_height * result_channels);

        // 计算第二张图片的缩放比例
        float scaleX = static_cast<float>(width2) / static_cast<float>(width1);
        float scaleY = static_cast<float>(height2) / static_cast<float>(height1);

        for (int y = 0; y < result_height; y++)
        {
            for (int x = 0; x < result_width; x++)
            {
                int result_index = (y * result_width + x) * 4;

                // 获取第一张图片的像素
                unsigned char r1 = image1_data[result_index + 0];
                unsigned char g1 = image1_data[result_index + 1];
                unsigned char b1 = image1_data[result_index + 2];
                unsigned char a1 = image1_data[result_index + 3];

                // 计算第二张图片的对应像素(使用双线性插值)
                float srcX = x * scaleX;
                float srcY = y * scaleY;

                // 边界检查
                if (srcX >= 0 && srcX < width2 && srcY >= 0 && srcY < height2)
                {
                    // 获取最近的四个像素进行双线性插值
                    int x0 = static_cast<int>(srcX);
                    int y0 = static_cast<int>(srcY);
                    int x1 = std::min(x0 + 1, width2 - 1);
                    int y1 = std::min(y0 + 1, height2 - 1);

                    float fx = srcX - x0;
                    float fy = srcY - y0;

                    // 获取四个角的像素
                    int idx00 = (y0 * width2 + x0) * 4;
                    int idx01 = (y0 * width2 + x1) * 4;
                    int idx10 = (y1 * width2 + x0) * 4;
                    int idx11 = (y1 * width2 + x1) * 4;

                    // 双线性插值
                    float r2 = (1 - fx) * (1 - fy) * image2_data[idx00 + 0] + fx * (1 - fy) * image2_data[idx01 + 0] +
                               (1 - fx) * fy * image2_data[idx10 + 0] + fx * fy * image2_data[idx11 + 0];
                    float g2 = (1 - fx) * (1 - fy) * image2_data[idx00 + 1] + fx * (1 - fy) * image2_data[idx01 + 1] +
                               (1 - fx) * fy * image2_data[idx10 + 1] + fx * fy * image2_data[idx11 + 1];
                    float b2 = (1 - fx) * (1 - fy) * image2_data[idx00 + 2] + fx * (1 - fy) * image2_data[idx01 + 2] +
                               (1 - fx) * fy * image2_data[idx10 + 2] + fx * fy * image2_data[idx11 + 2];
                    float a2 = (1 - fx) * (1 - fy) * image2_data[idx00 + 3] + fx * (1 - fy) * image2_data[idx01 + 3] +
                               (1 - fx) * fy * image2_data[idx10 + 3] + fx * fy * image2_data[idx11 + 3];

                    // 混合两张图片
                    result_data[result_index + 0] = static_cast<unsigned char>(r1 * (1 - alpha) + r2 * alpha);
                    result_data[result_index + 1] = static_cast<unsigned char>(g1 * (1 - alpha) + g2 * alpha);
                    result_data[result_index + 2] = static_cast<unsigned char>(b1 * (1 - alpha) + b2 * alpha);
                    result_data[result_index + 3] = static_cast<unsigned char>(a1 * (1 - alpha) + a2 * alpha);
                }
                else
                {
                    // 如果第二张图片的像素超出范围, 只使用第一张图片
                    result_data[result_index + 0] = r1;
                    result_data[result_index + 1] = g1;
                    result_data[result_index + 2] = b1;
                    result_data[result_index + 3] = a1;
                }
            }
        }

        GL_INFO("成功混合图片，混合比例: {}", alpha);
        return true;
    }

    // 保存结果图片
    bool SaveResult(const std::string &filename)
    {
        if (result_data.empty())
        {
            GL_ERROR("没有混合结果可以保存");
            return false;
        }

        if (!stbi_write_png(filename.c_str(), result_width, result_height, result_channels, result_data.data(), result_width * result_channels))
        {
            GL_ERROR("无法保存结果图片: {}", filename);
            return false;
        }

        GL_INFO("成功保存结果图片: {} ({}x{}x{})", filename, result_width, result_height, result_channels);
        return true;
    }

    // 获取图片信息
    void PrintImageInfo() const
    {
        GL_INFO("图片1: {}x{}x{}", width1, height1, channels1);
        GL_INFO("图片2: {}x{}x{}", width2, height2, channels2);
        if (!result_data.empty())
        {
            GL_INFO("结果: {}x{}x{}", result_width, result_height, result_channels);
        }
    }
};

void Fuse()
{
    // 直接在代码里设置图片路径
    std::string image1_path = ""; // 修改为你的第一张图片路径
    std::string image2_path = ""; // 修改为你的第二张图片路径
    std::string output_path = ""; // 修改为你的输出图片路径
    float alpha = 0.5f;           // 混合比例，第二张图片的权重 (0.0-1.0)

    GL_INFO("开始图像混合程序");
    GL_INFO("图片1: {}", image1_path);
    GL_INFO("图片2: {}", image2_path);
    GL_INFO("输出: {}", output_path);
    GL_INFO("混合比例: {}", alpha);

    ImageBlender blender;

    // 加载图片
    if (!blender.LoadImage1(image1_path))
    {
        return;
    }

    if (!blender.LoadImage2(image2_path))
    {
        return;
    }

    // 打印图片信息
    blender.PrintImageInfo();

    // 混合图片
    if (!blender.BlendImages(alpha))
    {
        return;
    }

    // 保存结果
    if (!blender.SaveResult(output_path))
    {
        return;
    }

    GL_INFO("图像混合完成！");
}

void FuseAll()
{
    // 设置两个输入文件夹路径和输出文件夹路径
    std::string folder1 = "";      // 第一张图片所在文件夹
    std::string folder2 = "";      // 第二张图片所在文件夹
    std::string outputFolder = ""; // 输出文件夹
    float alpha = 0.18f;           // 混合比例，第二张图片的权重 (0.0-1.0)

    GL_INFO("开始批量图像混合程序");
    GL_INFO("文件夹1: {}", folder1);
    GL_INFO("文件夹2: {}", folder2);
    GL_INFO("输出文件夹: {}", outputFolder);
    GL_INFO("混合比例: {}", alpha);

    // 获取两个文件夹中的图片文件
    std::vector<std::string> files1 = GetImageFiles(folder1);
    std::vector<std::string> files2 = GetImageFiles(folder2);

    if (files1.empty() || files2.empty())
    {
        GL_ERROR("没有找到图片文件");
        return;
    }

    if (files1.size() != files2.size())
    {
        GL_WARN("两个文件夹中的图片数量不匹配: 文件夹1有 {} 张, 文件夹2有 {} 张", files1.size(), files2.size());
        GL_WARN("将处理较少数量的图片: {}", std::min(files1.size(), files2.size()));
    }

    // 创建输出文件夹(如果不存在)
    if (!fs::exists(outputFolder))
    {
        fs::create_directories(outputFolder);
        GL_INFO("创建输出文件夹: {}", outputFolder);
    }

    // 处理对应序号的图片
    size_t processCount = std::min(files1.size(), files2.size());
    ImageBlender blender;

    for (size_t i = 0; i < processCount; ++i)
    {
        GL_INFO("处理第 {} 对图片 (共 {} 对)", i + 1, processCount);
        GL_INFO("图片1: {}", files1[i]);
        GL_INFO("图片2: {}", files2[i]);

        // 加载图片
        if (!blender.LoadImage1(files1[i]))
        {
            GL_ERROR("跳过第 {} 对图片 - 无法加载图片1", i + 1);
            continue;
        }

        if (!blender.LoadImage2(files2[i]))
        {
            GL_ERROR("跳过第 {} 对图片 - 无法加载图片2", i + 1);
            continue;
        }

        // 打印图片信息
        blender.PrintImageInfo();

        // 混合图片
        if (!blender.BlendImages(alpha))
        {
            GL_ERROR("跳过第 {} 对图片 - 混合失败", i + 1);
            continue;
        }

        // 生成输出文件名(保持原始文件名, 但放在输出文件夹中)
        fs::path file1Path(files1[i]);
        std::string outputFilename = (fs::path(outputFolder) / file1Path.filename()).string();

        // 保存结果
        if (!blender.SaveResult(outputFilename))
        {
            GL_ERROR("跳过第 {} 对图片 - 保存失败", i + 1);
            continue;
        }

        GL_INFO("成功保存混合结果: {}", outputFilename);
    }

    GL_INFO("批量图像混合完成！共处理了 {} 对图片", processCount);
}

int main()
{
    core::Logger::Init();
    Fuse();
    return 0;
}