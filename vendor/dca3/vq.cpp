#include "vq.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <random>
#include <limits>
#include <cstdint>
#include <tuple>
#include <array>

uint32_t twiddle_slow(uint32_t x,uint32_t y,uint32_t x_sz,uint32_t y_sz)
{
	uint32_t rv=0;//low 2 bits are directly passed  -> needs some misc stuff to work.However
			 //Pvr internally maps the 64b banks "as if" they were twiddled :p

	uint32_t sh=0;
	x_sz>>=1;
	y_sz>>=1;
	while(x_sz!=0 || y_sz!=0)
	{
		if (y_sz)
		{
			uint32_t temp=y&1;
			rv|=temp<<sh;

			y_sz>>=1;
			y>>=1;
			sh++;
		}
		if (x_sz)
		{
			uint32_t temp=x&1;
			rv|=temp<<sh;

			x_sz>>=1;
			x>>=1;
			sh++;
		}
	}	
	return rv;
}

// Color structure definition
bool Color::operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
}


// Hash function for Color to be used in unordered_map
namespace std {
    template<>
    struct hash<Color> {
        size_t operator()(const Color& color) const {
            return ((color.r * 73856093) ^ (color.g * 19349663) ^ (color.b * 83492791) ^ (color.a * 982451653));
        }
    };
}


// Convert ARGB8888 to ARGB4444
Color convertToARGB4444(const Color& color) {
    return {
        static_cast<uint8_t>((color.r & 0xF0) | (color.r >> 4)),
        static_cast<uint8_t>((color.g & 0xF0) | (color.g >> 4)),
        static_cast<uint8_t>((color.b & 0xF0) | (color.b >> 4)),
        static_cast<uint8_t>((color.a & 0xF0) | (color.a >> 4))
    };
}

// Convert ARGB8888 to ARGB1555
Color convertToARGB1555(const Color& color) {
    return {
        static_cast<uint8_t>((color.r & 0xF8) | (color.r >> 5)),
        static_cast<uint8_t>((color.g & 0xF8) | (color.g >> 5)),
        static_cast<uint8_t>((color.b & 0xF8) | (color.b >> 5)),
        static_cast<uint8_t>(color.a >= 0x80 ? 0xFF : 0)
    };
}

// Convert ARGB8888 to RGB565
Color convertToRGB565(const Color& color) {
    return {
        static_cast<uint8_t>((color.r & 0xF8) | (color.r >> 5)),
        static_cast<uint8_t>((color.g & 0xFC) | (color.g >> 6)),
        static_cast<uint8_t>((color.b & 0xF8) | (color.b >> 5)),
        color.a // Preserve alpha
    };
}

// Pack ARGB4444 to 16-bit
uint16_t packARGB4444(const Color& color) {
    return ((color.a & 0xF0) << 8) | ((color.r & 0xF0) << 4) | (color.g & 0xF0) | (color.b >> 4);
}

// Pack ARGB1555 to 16-bit
uint16_t packARGB1555(const Color& color) {
    return ((color.a & 0x80) << 8) | ((color.r & 0xF8) << 7) | ((color.g & 0xF8) << 2) | (color.b >> 3);
}

// Pack RGB565 to 16-bit
uint16_t packRGB565(const Color& color) {
    return ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3);
}

// Utility function to convert a 2x2 block of colors into a single uint64_t for easy comparison
uint64_t blockToKey(const std::array<Color, 4>& block) {
    uint64_t key = 0;
    for (int i = 0; i < 4; ++i) {
        key |= (static_cast<uint64_t>(block[i].r) << (i * 16 + 0));
        key |= (static_cast<uint64_t>(block[i].g) << (i * 16 + 4));
        key |= (static_cast<uint64_t>(block[i].b) << (i * 16 + 8));
        key |= (static_cast<uint64_t>(block[i].a) << (i * 16 + 12));
    }
    return key;
}

// Utility function to compute the Euclidean distance between two colors
float colorDistance(const Color& c1, const Color& c2) {
    int dr = c1.r - c2.r;
    int dg = c1.g - c2.g;
    int db = c1.b - c2.b;
    int da = c1.a - c2.a;
    return std::sqrt(dr * dr + dg * dg + db * db + da * da);
}

// Function to find the nearest codebook entry for a given block
int findNearestBlock(const std::array<Color, 4>& block, const std::vector<std::array<Color, 4>>& codebook, int codebookSize) {
    int nearestIndex = 0;
    float nearestDistance = std::numeric_limits<float>::max();
    auto firstEntry = 256 - codebookSize;
    for (int i = firstEntry; i < codebook.size(); ++i) {
        float distance = 0;
        for (int j = 0; j < 4; ++j) {
            distance += colorDistance(block[j], codebook[i][j]);
        }
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestIndex = i;
        }
    }
    return nearestIndex;
}

std::vector<std::array<Color, 4>> createCodebook(const std::vector<std::array<Color, 4>>& blocks, int codebookSize) {
    std::vector<std::array<Color, 4>> codebook(256);

    // Calculate the average color for each block
    std::vector<std::pair<std::array<Color, 4>, Color>> blocksWithAverages;
    for (const auto& block : blocks) {
        Color average = {};
        for (const auto& color : block) {
            average.r += color.r;
            average.g += color.g;
            average.b += color.b;
            average.a += color.a;
        }
        average.r /= 4;
        average.g /= 4;
        average.b /= 4;
        average.a /= 4;
        blocksWithAverages.push_back({block, average});
    }

    // Sort blocks by their average color
    std::sort(blocksWithAverages.begin(), blocksWithAverages.end(), [](const auto& a, const auto& b) {
        const Color& c1 = a.second;
        const Color& c2 = b.second;
        return (c1.r + c1.g + c1.b + c1.a) < (c2.r + c2.g + c2.b + c2.a);
    });

    auto firstEntry = 256 - codebookSize;
    // Select up to 256 evenly spaced blocks to form the codebook
    for (int i = firstEntry; i < 256; ++i) {
        codebook[i] = blocksWithAverages[(i-firstEntry) * (blocksWithAverages.size() / codebookSize)].first;
    }

    return codebook;
}

// Function to compress an image to VQ format
template<auto ColorConvert, auto ColorPack>
std::vector<uint8_t> compressToVQ(const std::vector<Color>& image, int width, int height, int codebookSize) {
    // Extract 2x2 blocks from the image
    std::vector<std::array<Color, 4>> blocks;
    blocks.resize(width*height/4);
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x += 2) {
            auto block_index = twiddle_slow(x, y, width, height);
            blocks[block_index>>2][0] = ColorConvert(image[x + y *width]);
            blocks[block_index>>2][1] = ColorConvert(image[x + (y+1) *width]);
            blocks[block_index>>2][2] = ColorConvert(image[x+1 + y*width]);
            blocks[block_index>>2][3] = ColorConvert(image[x+1 + (y+1) *width]);
        }
    }
    
    // Create the codebook
    auto codebook = createCodebook(blocks, codebookSize);
    
    // Create the index table
    std::vector<uint8_t> indexTable(blocks.size());
    for (int i = 0; i < blocks.size(); ++i) {
        indexTable[i] = findNearestBlock(blocks[i], codebook, codebookSize);
    }
    
    // Prepare the output
    std::vector<uint8_t> output;
    auto firstEntry = 256 - codebookSize;
    for (int i = firstEntry; i < 256; i++) {
        for (const auto& color : codebook[i]) {
            uint16_t packed = ColorPack(color);
            output.push_back(packed & 0xFF);
            output.push_back(packed >> 8);
        }
    }
    output.insert(output.end(), indexTable.begin(), indexTable.end());

    return output;
}

// Explicit template specializations
std::vector<uint8_t> compressToVQ_ARGB4444(const std::vector<Color>& image, int width, int height, int codebookSize) {
    return compressToVQ<convertToARGB4444, packARGB4444>(image, width, height, codebookSize);
}

std::vector<uint8_t> compressToVQ_ARGB1555(const std::vector<Color>& image, int width, int height, int codebookSize) {
    return compressToVQ<convertToARGB1555, packARGB1555>(image, width, height, codebookSize);
}

std::vector<uint8_t> compressToVQ_RGB565(const std::vector<Color>& image, int width, int height, int codebookSize) {
    return compressToVQ<convertToRGB565, packRGB565>(image, width, height, codebookSize);
}


// Function to create image from ARGB8888 data
std::vector<Color> createImageFromData_ARGB8888(const uint8_t* pixels, int width, int height, size_t stride) {
    std::vector<Color> image(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t* pixel = reinterpret_cast<const uint32_t*>(pixels + y * stride + x * 4);
            image[y * width + x] = {
                static_cast<uint8_t>(*pixel & 0xFF),         // r
                static_cast<uint8_t>((*pixel >> 8) & 0xFF),  // g
                static_cast<uint8_t>((*pixel >> 16) & 0xFF), // b
                static_cast<uint8_t>((*pixel >> 24) & 0xFF)  // a
            };
        }
    }
    return image;
}

// Function to create image from RGB888 data
std::vector<Color> createImageFromData_RGB888(const uint8_t* pixels, int width, int height, size_t stride) {
    std::vector<Color> image(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint8_t* pixel = pixels + y * stride + x * 3;
            image[y * width + x] = {
                pixel[0], // r
                pixel[1], // g
                pixel[2], // b
                255       // a (default alpha)
            };
        }
    }
    return image;
}

// Function to create image from RGB1555 data
std::vector<Color> createImageFromData_ARGB1555(const uint8_t* pixels, int width, int height, size_t stride) {
    std::vector<Color> image(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint16_t* pixel = reinterpret_cast<const uint16_t*>(pixels + y * stride + x * 2);
            image[y * width + x] = {
                static_cast<uint8_t>((*pixel & 0x1F) * 8),         // r
                static_cast<uint8_t>(((*pixel >> 5) & 0x1F) * 8),  // g
                static_cast<uint8_t>(((*pixel >> 10) & 0x1F) * 8), // b
                static_cast<uint8_t>(((*pixel >> 15) & 0x1) * 255) // a
            };
        }
    }
    return image;
}

std::vector<Color> downscaleImage(const std::vector<Color>& image, int width, int height, int newWidth, int newHeight) {
    int scaleFactorX = width / newWidth;
    int scaleFactorY = height / newHeight;
    std::vector<Color> downscaledImage(newWidth * newHeight);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int r = 0, g = 0, b = 0, a = 0;
            int pixelCount = 0;

            for (int dy = 0; dy < scaleFactorY; ++dy) {
                for (int dx = 0; dx < scaleFactorX; ++dx) {
                    int srcX = x * scaleFactorX + dx;
                    int srcY = y * scaleFactorY + dy;
                    if (srcX < width && srcY < height) {
                        Color c = image[srcY * width + srcX];
                        r += c.r;
                        g += c.g;
                        b += c.b;
                        a += c.a;
                        pixelCount++;
                    }
                }
            }

            downscaledImage[y * newWidth + x] = {
                static_cast<uint8_t>(r / pixelCount),
                static_cast<uint8_t>(g / pixelCount),
                static_cast<uint8_t>(b / pixelCount),
                static_cast<uint8_t>(a / pixelCount)
            };
        }
    }

    return downscaledImage;
}


// Function to pack colors to ARGB4444 format
std::vector<uint8_t> packColorsToARGB4444(const std::vector<Color>& colors, int width, int height) {
    std::vector<uint8_t> packedData(colors.size() * 2);

    auto j = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto i = j++;
            auto idx = twiddle_slow(x, y, width, height);
            uint16_t packedColor = (colors[i].a & 0xF0) << 8 |
                                (colors[i].r & 0xF0) << 4 |
                                (colors[i].g & 0xF0) |
                                (colors[i].b >> 4);

            packedData[idx * 2] = static_cast<uint8_t>(packedColor & 0xFF);
            packedData[idx * 2 + 1] = static_cast<uint8_t>((packedColor >> 8) & 0xFF);
        }
    }

    return packedData;
}

// Function to pack colors to RGB565 format
std::vector<uint8_t> packColorsToRGB565(const std::vector<Color>& colors, int width, int height) {
    std::vector<uint8_t> packedData(colors.size() * 2);

    auto j = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto i = j++;
            auto idx = twiddle_slow(x, y, width, height);
            uint16_t packedColor = (colors[i].r & 0xF8) << 8 |
                                (colors[i].g & 0xFC) << 3 |
                                (colors[i].b >> 3);

            packedData[idx * 2] = static_cast<uint8_t>(packedColor & 0xFF);
            packedData[idx * 2 + 1] = static_cast<uint8_t>((packedColor >> 8) & 0xFF);
        }
    }

    return packedData;
}

// Function to pack colors to ARGB1555 format
std::vector<uint8_t> packColorsToARGB1555(const std::vector<Color>& colors, int width, int height) {
    std::vector<uint8_t> packedData(colors.size() * 2);

    auto j = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto i = j++;
            auto idx = twiddle_slow(x, y, width, height);
            uint16_t packedColor = (colors[i].a & 0x80) << 8 |
                                (colors[i].r & 0xF8) << 7 |
                                (colors[i].g & 0xF8) << 2 |
                                (colors[i].b >> 3);

            packedData[idx * 2] = static_cast<uint8_t>(packedColor & 0xFF);
            packedData[idx * 2 + 1] = static_cast<uint8_t>((packedColor >> 8) & 0xFF);
        }
    }

    return packedData;
}