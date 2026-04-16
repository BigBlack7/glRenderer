#include "exrIO.hpp"
#include "utils/logger.hpp"

#include <tinyexr.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace core
{
    bool LoadExrImage(const std::filesystem::path &filePath, ExrImage &outImage) noexcept
    {
        outImage = ExrImage{};

        int width = 0;
        int height = 0;
        float *rgba = nullptr;
        const char *error = nullptr;

        const int result = LoadEXR(&rgba, &width, &height, filePath.string().c_str(), &error);
        if (result != TINYEXR_SUCCESS || rgba == nullptr)
        {
            GL_ERROR("[EXR] Load Failed: {} | {}", filePath.string(), error ? error : "Unknown");
            if (error)
                FreeEXRErrorMessage(error);
            return false;
        }

        if (error)
            FreeEXRErrorMessage(error);

        outImage.mWidth = width;
        outImage.mHeight = height;
        outImage.mChannels = 4;

        const size_t rowStride = static_cast<size_t>(width) * 4u;
        outImage.mPixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
        for (int y = 0; y < height; ++y)
        {
            const int srcY = height - 1 - y;
            std::memcpy(outImage.mPixels.data() + static_cast<size_t>(y) * rowStride,
                        rgba + static_cast<size_t>(srcY) * rowStride,
                        rowStride * sizeof(float));
        }

        free(rgba);
        return outImage.IsValid();
    }

    bool SaveExrImage(const std::filesystem::path &filePath,
                      const ExrImage &image,
                      ExrSavePixelType savePixelType) noexcept
    {
        if (!image.IsValid())
        {
            GL_ERROR("[EXR] Save Failed: Invalid Image Data | {}", filePath.string());
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);

        const int width = image.mWidth;
        const int height = image.mHeight;
        const int channels = image.mChannels;

        std::vector<float> chR(static_cast<size_t>(width) * static_cast<size_t>(height));
        std::vector<float> chG(static_cast<size_t>(width) * static_cast<size_t>(height));
        std::vector<float> chB(static_cast<size_t>(width) * static_cast<size_t>(height));
        std::vector<float> chA;
        if (channels == 4)
            chA.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

        for (int y = 0; y < height; ++y)
        {
            const int srcY = height - 1 - y;
            for (int x = 0; x < width; ++x)
            {
                const size_t i = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const size_t src = static_cast<size_t>(srcY) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const size_t base = src * static_cast<size_t>(channels);
                chR[i] = image.mPixels[base + 0u];
                chG[i] = image.mPixels[base + 1u];
                chB[i] = image.mPixels[base + 2u];
                if (channels == 4)
                    chA[i] = image.mPixels[base + 3u];
            }
        }

        EXRHeader header;
        InitEXRHeader(&header);
        EXRImage exr;
        InitEXRImage(&exr);

        exr.num_channels = channels;

        std::vector<float *> imagePtrs;
        imagePtrs.reserve(static_cast<size_t>(channels));
        if (channels == 4)
        {
            imagePtrs.push_back(chA.data());
            imagePtrs.push_back(chB.data());
            imagePtrs.push_back(chG.data());
            imagePtrs.push_back(chR.data());
        }
        else
        {
            imagePtrs.push_back(chB.data());
            imagePtrs.push_back(chG.data());
            imagePtrs.push_back(chR.data());
        }

        exr.images = reinterpret_cast<unsigned char **>(imagePtrs.data());
        exr.width = width;
        exr.height = height;

        header.num_channels = channels;
        header.channels = static_cast<EXRChannelInfo *>(malloc(sizeof(EXRChannelInfo) * static_cast<size_t>(channels)));
        header.pixel_types = static_cast<int *>(malloc(sizeof(int) * static_cast<size_t>(channels)));
        header.requested_pixel_types = static_cast<int *>(malloc(sizeof(int) * static_cast<size_t>(channels)));
        if (!header.channels || !header.pixel_types || !header.requested_pixel_types)
        {
            free(header.channels);
            free(header.pixel_types);
            free(header.requested_pixel_types);
            GL_ERROR("[EXR] Save Failed: OOM While Preparing Header | {}", filePath.string());
            return false;
        }

        if (channels == 4)
        {
            strncpy(header.channels[0].name, "A", 255);
            header.channels[0].name[255] = '\0';
            strncpy(header.channels[1].name, "B", 255);
            header.channels[1].name[255] = '\0';
            strncpy(header.channels[2].name, "G", 255);
            header.channels[2].name[255] = '\0';
            strncpy(header.channels[3].name, "R", 255);
            header.channels[3].name[255] = '\0';
        }
        else
        {
            strncpy(header.channels[0].name, "B", 255);
            header.channels[0].name[255] = '\0';
            strncpy(header.channels[1].name, "G", 255);
            header.channels[1].name[255] = '\0';
            strncpy(header.channels[2].name, "R", 255);
            header.channels[2].name[255] = '\0';
        }

        for (int i = 0; i < channels; ++i)
        {
            header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
            header.requested_pixel_types[i] = (savePixelType == ExrSavePixelType::Float32)
                                                  ? TINYEXR_PIXELTYPE_FLOAT
                                                  : TINYEXR_PIXELTYPE_HALF;
        }

        const char *error = nullptr;
        const int result = SaveEXRImageToFile(&exr, &header, filePath.string().c_str(), &error);

        free(header.channels);
        free(header.pixel_types);
        free(header.requested_pixel_types);

        if (result != TINYEXR_SUCCESS)
        {
            GL_ERROR("[EXR] Save Failed: {} | {}", filePath.string(), error ? error : "Unknown");
            if (error)
                FreeEXRErrorMessage(error);
            return false;
        }

        if (error)
            FreeEXRErrorMessage(error);

        return true;
    }
}
