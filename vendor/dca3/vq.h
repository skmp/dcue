#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct Color {
    uint8_t r, g, b, a;

    bool operator==(const Color& other) const;
};

std::vector<Color> createImageFromData_ARGB8888(const uint8_t* pixels, int width, int height, size_t stride);
std::vector<Color> createImageFromData_RGB888(const uint8_t* pixels, int width, int height, size_t stride);
std::vector<Color> createImageFromData_ARGB1555(const uint8_t* pixels, int width, int height, size_t stride);
std::vector<Color> downscaleImage(const std::vector<Color>& image, int width, int height, int newWidth, int newHeight);
std::vector<uint8_t> compressToVQ_ARGB4444(const std::vector<Color>& image, int width, int height, int codebookSize);
std::vector<uint8_t> compressToVQ_ARGB1555(const std::vector<Color>& image, int width, int height, int codebookSize);
std::vector<uint8_t> compressToVQ_RGB565(const std::vector<Color>& image, int width, int height, int codebookSize);
std::vector<uint8_t> packColorsToARGB4444(const std::vector<Color>& colors, int width, int height);
std::vector<uint8_t> packColorsToARGB1555(const std::vector<Color>& colors, int width, int height);
std::vector<uint8_t> packColorsToRGB565(const std::vector<Color>& colors, int width, int height);

std::vector<Color> decompressFromVQ_RGB565(const std::vector<uint8_t>& compressedData, int width, int height);