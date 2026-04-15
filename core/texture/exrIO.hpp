#pragma once
#include <filesystem>
#include <vector>

namespace core
{
    struct ExrImage final
    {
        int mWidth{0};
        int mHeight{0};
        int mChannels{0};             // 3 or 4
        std::vector<float> mPixels{}; // tightly packed row-major float32

        [[nodiscard]] bool IsValid() const noexcept
        {
            return mWidth > 0 && mHeight > 0 && (mChannels == 3 || mChannels == 4) &&
                   mPixels.size() == static_cast<size_t>(mWidth) * static_cast<size_t>(mHeight) * static_cast<size_t>(mChannels);
        }
    };

    bool LoadExrImage(const std::filesystem::path &filePath, ExrImage &outImage) noexcept;
    bool SaveExrImage(const std::filesystem::path &filePath, const ExrImage &image) noexcept;
}
