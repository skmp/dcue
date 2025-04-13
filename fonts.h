#include <cstdint>
#include <cstdio>
#include <cstring>

#include "vendor/gldc/alloc.h"

#include "dcue/types-native.h"
using namespace native;

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
void load_pvr(const char *fname, texture_t* texture);

static const char fontChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+[]{}|;':\",.<>?/`~";
#define FONT_CHAR_COUNT (sizeof(fontChars) - 1)

struct font_char_t {
    struct {
        float u0, v0, u1, v1;
    };
    struct {
        float x0, y0, x1, y1;
    };
    float advance;
};

enum font_style_t {
    fs_Normal,
    fs_Bold,
};

struct font_t {
    texture_t* texture;
    font_char_t* chars;
    uint32_t size;
    font_style_t style;
};
void InitializeFonts();