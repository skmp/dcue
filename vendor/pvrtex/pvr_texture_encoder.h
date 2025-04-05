#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "pixel.h"
#include "pvr_texture.h"
#include "stb_image_resize.h"

typedef enum {
	PTE_MIP_NONE,
	PTE_MIP_QUALITY,
	PTE_MIP_FAST,
} pteMipGen;

typedef enum {
	PTE_FIX_NONE,
	PTE_FIX_UP,
	PTE_FIX_DOWN,
	PTE_FIX_NEAREST,
} pteFixSizeMethod;
typedef enum {
	PTE_FIX_MIP_NONE,	//Must be square
	PTE_FIX_MIP_NARROW_X2,	//If not square, take narrower dimension and double it (512x32->64x64)
	PTE_FIX_MIP_NARROW_X4,	//If not square, take narrower dimension and quadruple it (512x32->128x128)
	PTE_FIX_MIP_MAX,		//Take largest dimension (512x32->512x512)
	PTE_FIX_MIP_MIN,		//Take narrowest dimension (512x32->32x32)
} pteFixMipSizeMethod;


typedef uint8_t VQIndex;

typedef enum {
	//The values for following seven formats match up with the values used by the hardware
	PTE_ARGB1555,
	PTE_RGB565,
	PTE_ARGB4444,
	PTE_YUV,
	PTE_NORMAL,
	PTE_PALETTE_4B,
	PTE_PALETTE_8B,
	
	//The following are not real, supported PVR formats, but used internally by some functions
	PTE_YUV_TWID = PT_YUV_TWID,
	
	//The following cannot be used as a ptPixelFormat. They are used by pte* functions only
	PTE_ABGR8888,
	PTE_BUMP,	//Signals input is height map that needs to be converted to normal map
	PTE_AUTO,	//Selects RGB565, ARGB1555, or ARGB4444 automatically based on alpha of input image
	PTE_AUTO_YUV,	//Selects YUV, ARGB1555, or ARGB4444 automatically based on alpha of input image
} ptePixelFormat;

typedef struct {
	unsigned w, h;
	int channels;
	pxlABGR8888 *pixels;
} pteImage;
static inline size_t pteImgPixelCnt(const pteImage *img) {
	return img->w * img->h;
}
static inline size_t pteImgSize(const pteImage *img) {
	return img->w * img->h * sizeof(pxlABGR8888);
}

typedef struct PvrTexEncoder {
//
//	The following should be set by the user before using the encoder
//
	
	//If true, generating a nontwiddled stride texture.
	//Texture width can 8, 16, or be any multiple of 32 that is less than or equal to 1024
	//If false, texture will be twiddled
	bool stride;		
	
	//Method of generating mipmaps, or no mipmaps generated
	pteMipGen want_mips;
	
	//Number of mipmap levels, always 1 if no mipmaps
	//Not set to final mipmap count until mipmaps have been generated
	unsigned mip_cnt;
	
	//Color format of texture
	ptePixelFormat pixel_format;
	
	//Size of codebook in indicies, ranges from 0 to 256
	//0 means uncompressed, >0 will result in a VQ compressed texture
	unsigned codebook_size;
	
	//Offset (in entries) into full codebook where the first element is
	//If you have a 128 entry codebook, and want to use the first half, set this to zero
	//If you want to the last half, set this to 128
	//codebook_size + pvr_idx_offset must be <= 256
	unsigned pvr_idx_offset;
	
	unsigned perfect_mips;	//number of mipmap levels to generate losslessly
	bool mip_shift_correction;	//preform correction for mipmap shifting
	
	//mips between top-high_weight and perfect_mips are given extra weight (quality) when compressing, 
	//0 means no high weight, 1 means all mips below highest have extra priority,
	//2 means all mips below second highest are given prio, etc.
	unsigned high_weight_mips;
	
	//Resize method to nearest valid size
	pteFixSizeMethod resize;
	
	//Resize method to make square
	pteFixMipSizeMethod mipresize;
	
	//Amount of dithering, 0 is none, 1 is full
	float dither;
	
	//How to downsample on the edges
	stbir_edge edge_method;
	
	//Generate small codebook size based on texture dimensions
	bool auto_small_vq;
	
	//Unprocessed source images specified by user
	unsigned src_img_cnt;
	pteImage src_imgs[PVR_MAX_MIPMAPS];
	
	float rgb_gamma;
	float alpha_gamma;
	
//
//	Below here is used internally by encoder. User should avoid messing with most of these.
//
	//Width and height of texture in pixels (if we have mipmaps, this is largest mip level)
	unsigned w, h;
	
	//If true, raw_mips contains twiddled data, otherwise data is normal, linear
	bool raw_is_twiddled;
		
	unsigned palette_size;	//size in colors, ranges from 0 to 256
	pxlABGR8888 *palette;
	
	//Preview with all mips in one image
	//height is h
	unsigned final_preview_w;
	pxlABGR8888 *final_preview;
	
	//Codebook in pvr format, uses pixel_format pixel
	void *pvr_codebook;
	
	//PVR texture data, in the same format used by the pvr, including all mipmaps laid out in order, including padding
	//If compressed, this is just the indices, and does NOT include the codebook
	void *pvr_tex;
	
	//Uncompressed PVR texture data, as pvr_tex, but in 32-bit color. This is generated first, then pvr_tex is generated from it
	pxlABGR8888 *pvr_tex32;
	
	//For the following three *_mips arrays...
	//If mip_cnt > 1, 0 is 1x1, 1 is 2x2, 3 is 4x4...
	//If mip_cnt == 1, 0 is only level, and its size is equal to this->w, this->h
	
	//Uncompressed source, 4-bytes per pixel
	pxlABGR8888 *raw_mips[PVR_MAX_MIPMAPS];
	
	//Raw PVR data
	//If texture is not compresed, this is in pixel_format
	//If texture is compressed, these are indicies
	uint8_t *pvr_mips[PVR_MAX_MIPMAPS];
	
	//Output preview
	//What you get after compressing and reducing color depth
	pxlABGR8888 *preview_mips[PVR_MAX_MIPMAPS];
} PvrTexEncoder;

//For both FOR_EACH, width and height of current mipmap level are in variables mw and mh

//Go through each level from small to large
#define FOR_EACH_MIP(pte, mipidx) \
	for(int mipidx = 0, mw = pteHasMips(pte) ? 1 : pte->w, mh = pteHasMips(pte) ? 1 : pte->h; mipidx < pte->mip_cnt; mipidx++, mw <<= 1, mh <<= 1)

//Go through each level from large to small
#define FOR_EACH_MIP_REV(pte, mipidx) \
	for(int mipidx = pte->mip_cnt-1, mw = pte->w, mh = pte->h; mipidx >= 0; mipidx--, mw >>= 1, mh >>= 1)

static inline unsigned pteTopMipLvl(const PvrTexEncoder *pte) {
	return pte->mip_cnt - 1;
}

static inline bool pteHasMips(const PvrTexEncoder *pte) {
	return pte->want_mips != PTE_MIP_NONE;
}
static inline bool pteIsCompressed(const PvrTexEncoder *pte) {
	return pte->codebook_size > 0;
}
static inline bool pteIsStrided(const PvrTexEncoder *pte) {
	return pte->stride;
}
static inline bool pteIsPalettized(const PvrTexEncoder *pte) {
	return pte->pixel_format == PTE_PALETTE_4B || pte->pixel_format == PTE_PALETTE_8B;
}

void pteInit(PvrTexEncoder *pte);
void pteFree(PvrTexEncoder *pte);
void pteLoadFromFiles(PvrTexEncoder *pte, const char **fnames, unsigned filecnt);
void pteEncodeTexture(PvrTexEncoder *pte);
void pteMakeSquare(PvrTexEncoder *pte);
int pteSetSize(PvrTexEncoder *pte);
void pteSetCompressed(PvrTexEncoder *pte, int codebook_size);
void pteGeneratePreviews(PvrTexEncoder *pte);
void pteAutoSelectPixelFormat(PvrTexEncoder *pte);

///////////
void ErrorExitOn(int cond, const char *fmt, ...);
void ErrorExit(const char *fmt, ...) __attribute__((noreturn));

typedef enum pteLogLevel {
	//When changing these, make sure to update logtypes[] in pteLogLocV
	LOG_NONE,	//Disables logs, do not log to this type
	
	LOG_WARNING,	//Important warnings or errors
	LOG_COMPLETION,	//Encode completion
	LOG_PROGRESS,	//Progress of encoding
	LOG_INFO,	//Useful info on encoding
	
	LOG_ALL,	//Prints all normal user visible logs, do not log to this type
	
	LOG_DEBUG,	//Debug info. Must be the highest level, used as bounds check in pteLogLocV
} pteLogLevel;

#define pteLog(level, ...) //pteLogLoc(level, __FILE__, __LINE__, __VA_ARGS__)
extern void pteLogLoc(unsigned level, const char *file, unsigned line, const char *fmt, ...);

