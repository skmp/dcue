#pragma once

#include <algorithm>
#include <fstream>
#include <limits>
#include <cstdint>
#include <tuple>
#include <array>
#include <assert.h>
#include <dc/pvr.h>
#include "vq.h"
#include "vendor/gldc/alloc.h"

#include "dcue/types-native.h"

#include <vector>

#define texloadf(...) // printf(__VA_ARGS__)

enum downsampleModes {
    NONE,
    HALF,
    QUARTER
};

enum pvrEncoders {
    PVRTOOL,
    PVRTEX
};

// TGA Header Struct
#pragma pack(push, 1)
struct TGAHeader {
    uint8_t idLength = 0;           // Length of the image ID field
    uint8_t colorMapType = 0;       // Color map type (0 = no color map)
    uint8_t imageType = 2;          // Image type (2 = uncompressed true-color)
    uint16_t colorMapOffset = 0;    // First color map entry (not used)
    uint16_t colorMapLength = 0;    // Number of color map entries (not used)
    uint8_t colorMapDepth = 0;      // Color map entry size in bits (not used)
    uint16_t xOrigin = 0;           // X origin of the image
    uint16_t yOrigin = 0;           // Y origin of the image
    uint16_t width = 0;             // Image width
    uint16_t height = 0;            // Image height
    uint8_t pixelDepth = 0;         // Bits per pixel (24 or 32)
    uint8_t imageDescriptor = 0x20; // Image descriptor (top-left origin)
};
#pragma pack(pop)


struct PVRHeader
{
    uint8_t PVRT[4];
    uint32_t nTextureDataSize;
    uint8_t nPixelFormat;
    uint8_t nDataFormat;
    uint16_t reserved;
    uint16_t nWidth;
    uint16_t nHeight;
};

struct GlobalIndexHeader
{
    uint8_t GBIX[4];
    uint32_t dmy;
    uint32_t nCodebookSize;
};

#define MAX_VQ_CODEBOOK_SIZE 256

bool writeTGA(const char* filename, std::vector<Color> imageData, uint16_t width, uint16_t height, uint8_t depth) {
    // Ensure valid input
    assert(depth == 24 || depth == 32);

    // Prepare the TGA header
    TGAHeader header;
    header.width = static_cast<uint16_t>(width);
    header.height = static_cast<uint16_t>(height);
    header.pixelDepth = static_cast<uint8_t>(depth);

    // Open the file for binary writing
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Write the TGA header
    file.write(reinterpret_cast<const char*>(&header), sizeof(TGAHeader));

    // Write the image data in BGR(A) order
    for (const auto& pixel : imageData) {
        file.put(pixel.b);
        file.put(pixel.g);
        file.put(pixel.r);
        if (depth == 32) {
            file.put(pixel.a);
        }
    }

    file.close();
    return true;
}

struct pvr_tex {
    std::vector<uint8_t> data;
    uint32_t flags;
    uint32_t offs;
    uint8_t lw;
    uint8_t lh;
};

pvr_tex loadPVR(const char *fname) {
    FILE *tex = NULL;
    PVRHeader HDR;

    /* Open the PVR texture file */
    tex = fopen(fname, "rb");

    /* Read in the PVR texture file header */
    fread(&HDR, 1, sizeof(PVRHeader), tex);

    pvr_tex rv;

    rv.flags = PVR_TXRFMT_TWIDDLED | PVR_TXRFMT_VQ_ENABLE;
    rv.offs = 0;

    if( memcmp( HDR.PVRT, "GBIX", 4 ) == 0 )
    {
        GlobalIndexHeader *gbixHdr = (GlobalIndexHeader*)&HDR;
        texloadf("gbixHdr->nCodebookSize: %i\n", gbixHdr->nCodebookSize);
        if(gbixHdr->nCodebookSize > 0 && gbixHdr->nCodebookSize <= MAX_VQ_CODEBOOK_SIZE)
            rv.offs = (MAX_VQ_CODEBOOK_SIZE - gbixHdr->nCodebookSize) * 4 * 2;
        // Go Back 4 bytes and re-read teh PVR header
        fseek(tex, -4, SEEK_CUR);
        fread(&HDR, 1, sizeof(PVRHeader), tex);
    }

    // VQ or small VQ
    assert(HDR.nDataFormat == 0x3 || HDR.nDataFormat == 0x10);
    switch(HDR.nPixelFormat) {
        case 0x0: // ARGB1555
            rv.flags |= PVR_TXRFMT_ARGB1555;
            break;
        case 0x1: // RGB565
            rv.flags |= PVR_TXRFMT_RGB565;
            break;
        case 0x2: // ARGB4444
            rv.flags |= PVR_TXRFMT_ARGB4444;
            break;
        case 0x3: // YUV422
            rv.flags |= PVR_TXRFMT_YUV422;
            break;
        default:
            assert(false && "Invalid texture format");
            break;
    }

    rv.data.resize(HDR.nTextureDataSize-16);
    
    fread(rv.data.data(), 1, rv.data.size(), tex); /* Read in the PVR texture data */
    texloadf("PVR TEXTURE READ: %ix%i, %i, %i\n", HDR.nWidth, HDR.nHeight, rv.offs, rv.data.size());
    if (rv.data.size() >= 256) {
        assert((rv.data.size() & 255) == 0);
    } else {
        assert((rv.data.size() & 31) == 0);
    }

    fclose(tex);

    return rv;
}