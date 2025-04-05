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

#define texconvf(...)

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

#if 0
void loadPVR(char *fname, rw::Raster* raster, auto* natras, auto flags) {
    FILE *tex = NULL;
    PVRHeader HDR;

    /* Open the PVR texture file */
    tex = fopen(fname, "rb");

    /* Read in the PVR texture file header */
    fread(&HDR, 1, sizeof(PVRHeader), tex);

    natras->pvr_flags = 0;
    natras->pvr_flags |= flags;
    natras->texoffs = 0;

    if( memcmp( HDR.PVRT, "GBIX", 4 ) == 0 )
    {
        GlobalIndexHeader *gbixHdr = (GlobalIndexHeader*)&HDR;
        texconvf("gbixHdr->nCodebookSize: %i\n", gbixHdr->nCodebookSize);
        if(gbixHdr->nCodebookSize > 0 && gbixHdr->nCodebookSize <= MAX_VQ_CODEBOOK_SIZE)
            natras->texoffs = (MAX_VQ_CODEBOOK_SIZE - gbixHdr->nCodebookSize) * 4 * 2;
        // Go Back 4 bytes and re-read teh PVR header
        fseek(tex, -4, SEEK_CUR);
        fread(&HDR, 1, sizeof(PVRHeader), tex);
    }

    unsigned rasterFmt = 0;
    // VQ or small VQ
    assert(HDR.nDataFormat == 0x3 || HDR.nDataFormat == 0x10);
    switch(HDR.nPixelFormat) {
        case 0x0: // ARGB1555
            natras->pvr_flags |= PVR_TXRFMT_ARGB1555;
            rasterFmt = rw::Raster::C1555;
            break;
        case 0x1: // RGB565
            natras->pvr_flags |= PVR_TXRFMT_RGB565;
            rasterFmt = rw::Raster::C565;
            break;
        case 0x2: // ARGB4444
            natras->pvr_flags |= PVR_TXRFMT_ARGB4444;
            rasterFmt = rw::Raster::C4444;
            break;
        case 0x3: // YUV422
            natras->pvr_flags |= PVR_TXRFMT_YUV422;
            rasterFmt = rw::Raster::C565;  // this is a bit of a hack
            break;
        default:
            assert(false && "Invalid texture format");
            break;
    }

    raster->format &= ~0x0F00;
    raster->format |= rasterFmt;

    natras->texsize = HDR.nTextureDataSize-16;
    natras->texaddr = alloc_malloc(natras, natras->texsize);
    fread(natras->texaddr, 1, natras->texsize, tex); /* Read in the PVR texture data */
    texconvf("PVR TEXTURE READ: %ix%i, %i, %i\n", HDR.nWidth, HDR.nHeight, natras->texoffs, natras->texsize);
    if (natras->texsize >= 256) {
        assert((natras->texsize & 255) == 0);
    } else {
        assert((natras->texsize & 31) == 0);
    }

    fclose(tex);
}
#endif