#include <dc/pvr.h>
#include <dc/maple.h>
#include <dc/matrix.h>
#include <dc/maple/controller.h>
#include "dcue/types-native.h"
#include "vendor/gldc/alloc.h"

#include "vendor/dca3/float16.h"

#include "unistd.h"

#include <vector>
#include <fstream>
#include <iostream>

#include <cmath>
#include <cassert>
#include <cstring>
#include <chrono>
#include <map>

#include "components.h"
#include "components/animations.h"
#include "components/hierarchy.h"
#include "components/scripts.h"
#include "components/cameras.h"
#include "components/lights.h"
#include "components/physics.h"
#include "components/fonts.h"
#include "components/audio_sources.h"
#include "components/audio_clips.h"

#include "pavo/pavo.h"

#include "dcue/coroutines.h"

#if defined(DC_SIM)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb/stb_image_write.h"
#endif

float timeDeltaTime;

// #define DEBUG_PHYSICS
// #define DEBUG_LOOKAT

#define ARRAY_SIZE(array)                (sizeof(array) / sizeof(array[0]))

static const float deg2rad = M_PI/180.0f;

#define MAX_LIGHTS 8

#if defined(DC_SH4)
#define FLUSH_TA_DATA(src) do { __asm__ __volatile__("ocbwb @%0" : : "r" (src) : "memory"); } while(0)
#define mat_trans_nodiv_nomod(x, y, z, x2, y2, z2, w2) do { \
	register float __x __asm__("fr12") = (x); \
	register float __y __asm__("fr13") = (y); \
	register float __z __asm__("fr14") = (z); \
	register float __w __asm__("fr15") = 1.0f; \
	__asm__ __volatile__( "ftrv  xmtrx, fv12\n" \
						  : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w) \
						  : "0" (__x), "1" (__y), "2" (__z), "3" (__w) ); \
	x2 = __x; y2 = __y; z2 = __z; w2 = __w; \
} while(false)

#define mat_trans_nodiv_nomod_zerow(x, y, z, x2, y2, z2, w2) do { \
	register float __x __asm__("fr12") = (x); \
	register float __y __asm__("fr13") = (y); \
	register float __z __asm__("fr14") = (z); \
	register float __w __asm__("fr15") = 0.0f; \
	__asm__ __volatile__( "ftrv  xmtrx, fv12\n" \
						  : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w) \
						  : "0" (__x), "1" (__y), "2" (__z), "3" (__w) ); \
	x2 = __x; y2 = __y; z2 = __z; w2 = __w; \
} while(false)

#define mat_trans_w_nodiv_nomod(x, y, z, w) do { \
	register float __x __asm__("fr12") = (x); \
	register float __y __asm__("fr13") = (y); \
	register float __z __asm__("fr14") = (z); \
	register float __w __asm__("fr15") = 1.0f; \
	__asm__ __volatile__( "ftrv  xmtrx, fv12\n" \
						  : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w) \
						  : "0" (__x), "1" (__y), "2" (__z), "3" (__w) ); \
	w = __w; \
} while(false)
#else
#define dcache_pref_block(x) do { } while (false)
#define frsqrt(a) 				(1.0f/sqrtf(a))
#define FLUSH_TA_DATA(src) do { pvr_dr_commit(src); } while(0)

#define mat_trans_single3_nomod(x_, y_, z_, x2, y2, z2) do { \
    vector_t tmp = { x_, y_, z_, 1.0f }; \
    mat_transform(&tmp, &tmp, 1, 0); \
    z2 = 1.0f / tmp.w; \
    x2 = tmp.x * z2; \
    y2 = tmp.y * z2; \
} while(false)

#define mat_trans_nodiv_nomod(x_, y_, z_, x2, y2, z2, w2) do { \
    vector_t tmp1233123 = { x_, y_, z_, 1.0f }; \
    mat_transform(&tmp1233123, &tmp1233123, 1, 0); \
    x2 = tmp1233123.x; y2 = tmp1233123.y; z2 = tmp1233123.z; w2 = tmp1233123.w; \
} while(false)

#define mat_trans_nodiv_nomod_zerow(x_, y_, z_, x2, y2, z2, w2) do { \
    vector_t tmp1233123 = { x_, y_, z_, 0.0f }; \
    mat_transform(&tmp1233123, &tmp1233123, 1, 0); \
    x2 = tmp1233123.x; y2 = tmp1233123.y; z2 = tmp1233123.z; w2 = tmp1233123.w; \
} while(false)

#define mat_trans_w_nodiv_nomod(x_, y_, z_, w_) do { \
    vector_t tmp1233123 = { x_, y_, z_, 1.0f }; \
    mat_transform(&tmp1233123, &tmp1233123, 1, 0); \
    w_ = tmp1233123.w; \
} while(false)

#define memcpy4 memcpy
#endif

struct alignas(32) pvr_vertex64_t {
	uint32_t flags;			/**< \brief TA command (vertex flags) */
	float	 x;				/**< \brief X coordinate */
	float	 y;				/**< \brief Y coordinate */
	float	 z;				/**< \brief Z coordinate */
	union {
		struct {
			uint16_t v;		/**< \brief Texture V coordinate */
			uint16_t u;		/**< \brief Texture U coordinate */
		};
		uint32_t uv;
		float uf32;
	};
	union {
		uint32_t _dmy1;
		float vf32;
	};
	
	union {
		uint32_t _dmy2;
		float tex_z;
	};
	uint32_t _dmy3;
	float a;
	float r;
	float g;
	float b;
	float o_a;
	float o_r;
	float o_g;
	float o_b;	
};


struct alignas(32) pvr_vertex64_t1 {
	uint32_t flags;			/**< \brief TA command (vertex flags) */
	float	 x;				/**< \brief X coordinate */
	float	 y;				/**< \brief Y coordinate */
	float	 z;				/**< \brief Z coordinate */
	union {
		struct {
			uint16_t v;		/**< \brief Texture V coordinate */
			uint16_t u;		/**< \brief Texture U coordinate */
		};
		uint32_t uv;
	};
	float    dmy1;
	float    w;             // not real, just padding
	uint32_t dmy3;
};

struct alignas(32) pvr_vertex64_t2 {
	float a;
	float r;
	float g;
	float b;
	float o_a;
	float o_r;
	float o_g;
	float o_b;
};

struct alignas(32) pvr_vertex32_ut {
	uint32_t flags;			/**< \brief TA command (vertex flags) */
	float	 x;				/**< \brief X coordinate */
	float	 y;				/**< \brief Y coordinate */
	float	 z;				/**< \brief Z coordinate */
	float a;
	float r;
	float g;
	float b; 
};


static pvr_dr_state_t drState;

using namespace native;

#if !defined(DC_SIM)
#include <kos.h>
KOS_INIT_FLAGS(INIT_IRQ | INIT_CONTROLLER | INIT_CDROM | INIT_VMU);
#endif

static pvr_init_params_t pvr_params = {
	.opb_sizes = {
				PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_8, PVR_BINSIZE_0,
				PVR_BINSIZE_8
	},
	.autosort_disabled = true
};

std::vector<texture_t*> textures;
std::vector<material_t*> materials;
std::vector<material_t**> material_groups;
std::vector<mesh_t*> meshes;
std::vector<game_object_t*> gameObjects;

texture_t* skybox[6];
RGBAf skyboxTint;

void load_pvr(const char *fname, texture_t* texture) {
    FILE *tex = NULL;
    PVRHeader HDR;

    /* Open the PVR texture file */
    tex = fopen(fname, "rb");

    /* Read in the PVR texture file header */
    fread(&HDR, 1, sizeof(PVRHeader), tex);

    texture->flags = PVR_TXRFMT_TWIDDLED | PVR_TXRFMT_VQ_ENABLE;
    texture->offs = 0;

    if( memcmp( HDR.PVRT, "GBIX", 4 ) == 0 )
    {
        GlobalIndexHeader *gbixHdr = (GlobalIndexHeader*)&HDR;
        if(gbixHdr->nCodebookSize > 0 && gbixHdr->nCodebookSize <= 256)
        texture->offs = (256 - gbixHdr->nCodebookSize) * 4 * 2;
        // Go Back 4 bytes and re-read teh PVR header
        fseek(tex, -4, SEEK_CUR);
        fread(&HDR, 1, sizeof(PVRHeader), tex);
    }

    // VQ or small VQ
    assert(HDR.nDataFormat == 0x3 || HDR.nDataFormat == 0x10);
    switch(HDR.nPixelFormat) {
        case 0x0: // ARGB1555
            texture->flags |= PVR_TXRFMT_ARGB1555;
            break;
        case 0x1: // RGB565
            texture->flags |= PVR_TXRFMT_RGB565;
            break;
        case 0x2: // ARGB4444
            texture->flags |= PVR_TXRFMT_ARGB4444;
            break;
        case 0x3: // YUV422
            texture->flags |= PVR_TXRFMT_YUV422;
            break;
        default:
            assert(false && "Invalid texture format");
            break;
    }

    uint8_t temp[2048];
    
    size_t remaining = HDR.nTextureDataSize - 16;
    texture->data = (pvr_ptr_t)alloc_malloc(&texture->data, remaining);
    uintptr_t dst = (uintptr_t)texture->data;

    while (remaining > 0) {
        size_t to_read = remaining < sizeof(temp) ? remaining : sizeof(temp);
        assert(fread(temp, 1, to_read, tex) == to_read);
        
        memcpy((void*)dst, temp, to_read);
        remaining -= to_read;
        dst += to_read;
    }

    texture->lw = __builtin_ctz(HDR.nWidth) - 3;
    texture->lh = __builtin_ctz(HDR.nHeight) - 3;
	fclose(tex);
}

bool loadScene(const char* scene) {
    std::ifstream in(scene, std::ios::binary);
    if (!in) {
        std::cout << "Failed to open file: " << scene << std::endl;
        return false;
    }

    // Read and verify header (8 bytes)
    char header[9] = { 0};
    in.read(header, 8);
    if (strncmp(header, "DCUENS04", 8) != 0) {
        std::cout << "Invalid file header: " << header << std::endl;
        return false;
    }

    uint32_t tmp;

    // Read textures
    uint32_t textureCount;
    in.read(reinterpret_cast<char*>(&textureCount), sizeof(textureCount));
    textures.resize(textureCount);
    for (uint32_t i = 0; i < textureCount; ++i) {
        auto tex = textures[i] = new texture_t();
        in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        tex->data = alloc_malloc(&tex->data, tmp);
        in.read((char*)&tex->flags, sizeof(tex->flags));
		in.read((char*)&tex->offs, sizeof(tex->offs));
		in.read((char*)&tex->lw, sizeof(tex->lw));
		in.read((char*)&tex->lh, sizeof(tex->lh));
        in.read(reinterpret_cast<char*>(tex->data), tmp);
    }

    // Read materials
    uint32_t materialCount;
    in.read(reinterpret_cast<char*>(&materialCount), sizeof(materialCount));
    materials.resize(materialCount);
    for (uint32_t i = 0; i < materialCount; ++i) {
        auto material = materials[i] = new material_t();
        in.read((char*)&material->color.alpha, sizeof(material->color.alpha));
		in.read((char*)&material->color.red, sizeof(material->color.red));
		in.read((char*)&material->color.green, sizeof(material->color.green));
		in.read((char*)&material->color.blue, sizeof(material->color.blue));

		in.read((char*)&material->emission.alpha, sizeof(material->emission.alpha));
		in.read((char*)&material->emission.red, sizeof(material->emission.red));
		in.read((char*)&material->emission.green, sizeof(material->emission.green));
		in.read((char*)&material->emission.blue, sizeof(material->emission.blue));
        
        in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        if (tmp != UINT32_MAX) {
            assert(tmp < textures.size());
            material->texture = textures[tmp];
        } else {
            material->texture = nullptr;
        }
    }

    // Read meshes
    uint32_t meshCount;
    in.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));
    meshes.resize(meshCount);
    for (uint32_t i = 0; i < meshCount; ++i) {
		uint32_t quadDataSize;
		in.read(reinterpret_cast<char*>(&quadDataSize), sizeof(quadDataSize));
        in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        auto mesh = meshes[i] = (mesh_t*)malloc(sizeof(mesh_t) + tmp);
        in.read((char*)&mesh->bounding_sphere, sizeof(mesh->bounding_sphere));
		mesh->quadData = (uint8_t*)malloc(quadDataSize);
		in.read(reinterpret_cast<char*>(mesh->quadData), quadDataSize);
        in.read(reinterpret_cast<char*>(mesh->data), tmp);
    }

    // Read game objects
    uint32_t gameObjectCount;
    in.read(reinterpret_cast<char*>(&gameObjectCount), sizeof(gameObjectCount));
    gameObjects.resize(gameObjectCount);
    for (uint32_t i = 0; i < gameObjectCount; ++i) {
        auto go = gameObjects[i] = new game_object_t();
		bool active;
        in.read((char*)&active, sizeof(active));
		if (!active) {
			go->inactiveFlags = goi_inactive;
		}
        in.read(reinterpret_cast<char*>(&go->ltw.m00), 16 * sizeof(float));
        in.read((char*)&go->mesh_enabled, sizeof(go->mesh_enabled));

        in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        if (tmp != UINT32_MAX) {
            assert(tmp < meshes.size());
            go->mesh = meshes[tmp];
        } else {
            go->mesh = nullptr;
        }
        in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        auto submesh_count = tmp;
        if (submesh_count != 0) {
            auto materials_group = new material_t*[submesh_count];
            material_groups.push_back(materials_group);
            go->materials = materials_group;
            for (uint32_t materialNum = 0; materialNum < submesh_count; materialNum++) {
                in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
                if (tmp != UINT32_MAX) {
                    assert(tmp < materials.size());
                    go->materials[materialNum] = materials[tmp];
                } else {
                    go->materials[materialNum] = nullptr;
                }
            }
        } else {
            go->materials = nullptr;
        }
        go->submesh_count = submesh_count;
    }

	for (int i = 0; i < 6; i++) {
		in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
		skybox[i] = textures[tmp];
	}

	in.read(reinterpret_cast<char*>(&skyboxTint.alpha), sizeof(skyboxTint.alpha));
	in.read(reinterpret_cast<char*>(&skyboxTint.red), sizeof(skyboxTint.red));
	in.read(reinterpret_cast<char*>(&skyboxTint.green), sizeof(skyboxTint.green));
	in.read(reinterpret_cast<char*>(&skyboxTint.blue), sizeof(skyboxTint.blue));

	assert(!in.bad());
    in.close();
    printf("Loaded %d textures, %d materials, %d meshes, and %d game objects.\n",
           static_cast<int>(textures.size()), static_cast<int>(materials.size()),
           static_cast<int>(meshes.size()), static_cast<int>(gameObjects.size()));

    return true;
}

matrix_t DCE_MESHLET_MAT_DECODE = {
	{ 1.0f/256, 0.0f, 0.0f, 0.0f},
	{ 0.0f, 1.0f/256, 0.0f, 0.0f},
	{ 0.0f, 0.0f, 1.0f/256, 0.0f},
	{ 0.0f, 0.0f, 0.0f, 1.0f},
};

matrix_t DCE_MESHLET_MAT_VERTEX_COLOR = {
	{ 1.0f, 0.0f, 0.0f, 0.0f},
	{ 0.0f, 1.0f, 0.0f, 0.0f},
	{ 0.0f, 0.0f, 1.0f, 0.0f},
	{ 0.0f, 0.0f, 0.0f, 0.0f},
};

void dce_set_mat_decode(float scale, float x, float y, float z) {
    DCE_MESHLET_MAT_DECODE[0][0] = scale;
    DCE_MESHLET_MAT_DECODE[1][1] = scale;
    DCE_MESHLET_MAT_DECODE[2][2] = scale;

    DCE_MESHLET_MAT_DECODE[3][0] = x;
    DCE_MESHLET_MAT_DECODE[3][1] = y;
    DCE_MESHLET_MAT_DECODE[3][2] = z;
}

void dce_set_mat_vertex_color(const RGBAf* residual, const RGBAf* material) {
	DCE_MESHLET_MAT_VERTEX_COLOR[0][0] = material->blue;
	DCE_MESHLET_MAT_VERTEX_COLOR[1][1] = material->green;
	DCE_MESHLET_MAT_VERTEX_COLOR[2][2] = material->red;

	DCE_MESHLET_MAT_VERTEX_COLOR[3][0] = 128 * material->blue + residual->blue;
	DCE_MESHLET_MAT_VERTEX_COLOR[3][1] = 128 * material->green + residual->green;
	DCE_MESHLET_MAT_VERTEX_COLOR[3][2] = 128 * material->red + residual->red;
	DCE_MESHLET_MAT_VERTEX_COLOR[3][3] = residual->alpha;
}

// void DCE_MatrixViewport(float x, float y, float width, float height) {
//     DCE_MAT_SCREENVIEW[0][0] = -width * 0.5f;
//     DCE_MAT_SCREENVIEW[1][1] = height * 0.5f;
//     DCE_MAT_SCREENVIEW[2][2] = 1;
//     DCE_MAT_SCREENVIEW[3][0] = -DCE_MAT_SCREENVIEW[0][0] + x;
//     DCE_MAT_SCREENVIEW[3][1] = height - (DCE_MAT_SCREENVIEW[1][1] + y); 
// }

// These /really/ depend on the compiler to optimize the constants out in order to be fast
// Ugly, but works
// Will rewrite later on to be more optimized

/* Compile a polygon context into a polygon header */
inline void pvr_poly_compile_fast(pvr_poly_hdr_t *dst, pvr_poly_cxt_t *src) {
    int u, v;
    uint32  txr_base;

    /* Basically we just take each parameter, clip it, shift it
       into place, and OR it into the final result. */

    /* The base values for CMD */
    dst->cmd = PVR_CMD_POLYHDR;

    if(src->txr.enable == PVR_TEXTURE_ENABLE)
        dst->cmd |= 8;

    /* Or in the list type, shading type, color and UV formats */
    dst->cmd |= (src->list_type << PVR_TA_CMD_TYPE_SHIFT);// & PVR_TA_CMD_TYPE_MASK;
    dst->cmd |= (src->fmt.color << PVR_TA_CMD_CLRFMT_SHIFT);// & PVR_TA_CMD_CLRFMT_MASK;
    dst->cmd |= (src->gen.shading << PVR_TA_CMD_SHADE_SHIFT);// & PVR_TA_CMD_SHADE_MASK;
    dst->cmd |= (src->fmt.uv << PVR_TA_CMD_UVFMT_SHIFT);// & PVR_TA_CMD_UVFMT_MASK;
    dst->cmd |= (src->gen.clip_mode << PVR_TA_CMD_USERCLIP_SHIFT);// & PVR_TA_CMD_USERCLIP_MASK;
    dst->cmd |= (src->fmt.modifier << PVR_TA_CMD_MODIFIER_SHIFT);// & PVR_TA_CMD_MODIFIER_MASK;
    dst->cmd |= (src->gen.modifier_mode << PVR_TA_CMD_MODIFIERMODE_SHIFT);// & PVR_TA_CMD_MODIFIERMODE_MASK;
    dst->cmd |= (src->gen.specular << PVR_TA_CMD_SPECULAR_SHIFT);// & PVR_TA_CMD_SPECULAR_MASK;

    /* Polygon mode 1 */
    dst->mode1  = (src->depth.comparison << PVR_TA_PM1_DEPTHCMP_SHIFT);// & PVR_TA_PM1_DEPTHCMP_MASK;
    dst->mode1 |= (src->gen.culling << PVR_TA_PM1_CULLING_SHIFT);// & PVR_TA_PM1_CULLING_MASK;
    dst->mode1 |= (src->depth.write << PVR_TA_PM1_DEPTHWRITE_SHIFT);// & PVR_TA_PM1_DEPTHWRITE_MASK;
    dst->mode1 |= (src->txr.enable << PVR_TA_PM1_TXRENABLE_SHIFT);// & PVR_TA_PM1_TXRENABLE_MASK;

    /* Polygon mode 2 */
    dst->mode2  = (src->blend.src << PVR_TA_PM2_SRCBLEND_SHIFT);// & PVR_TA_PM2_SRCBLEND_MASK;
    dst->mode2 |= (src->blend.dst << PVR_TA_PM2_DSTBLEND_SHIFT);// & PVR_TA_PM2_DSTBLEND_MASK;
    dst->mode2 |= (src->blend.src_enable << PVR_TA_PM2_SRCENABLE_SHIFT);// & PVR_TA_PM2_SRCENABLE_MASK;
    dst->mode2 |= (src->blend.dst_enable << PVR_TA_PM2_DSTENABLE_SHIFT);// & PVR_TA_PM2_DSTENABLE_MASK;
    dst->mode2 |= (src->gen.fog_type << PVR_TA_PM2_FOG_SHIFT);// & PVR_TA_PM2_FOG_MASK;
    dst->mode2 |= (src->gen.color_clamp << PVR_TA_PM2_CLAMP_SHIFT);// & PVR_TA_PM2_CLAMP_MASK;
    dst->mode2 |= (src->gen.alpha << PVR_TA_PM2_ALPHA_SHIFT);// & PVR_TA_PM2_ALPHA_MASK;

    if(src->txr.enable == PVR_TEXTURE_DISABLE) {
        dst->mode3 = 0;
    }
    else {
        dst->mode2 |= (src->txr.alpha << PVR_TA_PM2_TXRALPHA_SHIFT);// & PVR_TA_PM2_TXRALPHA_MASK;
        dst->mode2 |= (src->txr.uv_flip << PVR_TA_PM2_UVFLIP_SHIFT);// & PVR_TA_PM2_UVFLIP_MASK;
        dst->mode2 |= (src->txr.uv_clamp << PVR_TA_PM2_UVCLAMP_SHIFT);// & PVR_TA_PM2_UVCLAMP_MASK;
        dst->mode2 |= (src->txr.filter << PVR_TA_PM2_FILTER_SHIFT);// & PVR_TA_PM2_FILTER_MASK;
        dst->mode2 |= (src->txr.mipmap_bias << PVR_TA_PM2_MIPBIAS_SHIFT);// & PVR_TA_PM2_MIPBIAS_MASK;
        dst->mode2 |= (src->txr.env << PVR_TA_PM2_TXRENV_SHIFT);// & PVR_TA_PM2_TXRENV_MASK;

		u = src->txr.width;
		v = src->txr.height;

        dst->mode2 |= (u << PVR_TA_PM2_USIZE_SHIFT);// & PVR_TA_PM2_USIZE_MASK;
        dst->mode2 |= (v << PVR_TA_PM2_VSIZE_SHIFT);// & PVR_TA_PM2_VSIZE_MASK;

        /* Polygon mode 3 */
        dst->mode3  = (src->txr.mipmap << PVR_TA_PM3_MIPMAP_SHIFT);// & PVR_TA_PM3_MIPMAP_MASK;
        dst->mode3 |= (src->txr.format << PVR_TA_PM3_TXRFMT_SHIFT);// & PVR_TA_PM3_TXRFMT_MASK;

        /* Convert the texture address */
		#if defined(DC_SIM)
		txr_base = (ptr_t)src->txr.base - (ptr_t)emu_vram;
		#else
        txr_base = (ptr_t)src->txr.base;
		#endif
        txr_base = (txr_base & 0x00fffff8) >> 3;
        dst->mode3 |= txr_base;
    }
}

/* Create a colored polygon context with parameters similar to
   the old "ta" function `ta_poly_hdr_col' */
void pvr_poly_cxt_col_fast(pvr_poly_hdr_t *hdr, pvr_list_t list,
				int fmt_color,
				// isMatFX ? PVR_BLEND_SRCALPHA : doBlend ? srcBlend : PVR_BLEND_ONE,
					int blend_src,
				// isMatFX ? PVR_BLEND_INVSRCALPHA : doBlend ? dstBlend : PVR_BLEND_ZERO,
					int blend_dst,
				// zFunction,
					int depth_comparison,
				// zWrite,
					int depth_write,
				// cullMode == CULLNONE ? PVR_CULLING_SMALL : cullMode == CULLBACK ? PVR_CULLING_CW : PVR_CULLING_CCW,
					int gen_culling,
				// fogEnabled ? PVR_FOG_TABLE : PVR_FOG_DISABLE
					int gen_fog_type
				) {
    int alpha;
	pvr_poly_cxt_t cxt;
	pvr_poly_cxt_t *dst = &cxt;

    /* Start off blank */
    memset(dst, 0, sizeof(pvr_poly_cxt_t));

    /* Fill in a few values */
    dst->list_type = list;
    alpha = list > PVR_LIST_OP_MOD;
    dst->fmt.color = fmt_color;
    dst->fmt.uv = PVR_UVFMT_32BIT;
    dst->gen.shading = PVR_SHADE_GOURAUD;
    dst->depth.comparison = depth_comparison;
    dst->depth.write = depth_write;
    dst->gen.culling = gen_culling;
    dst->txr.enable = PVR_TEXTURE_DISABLE;

    if(!alpha) {
        dst->gen.alpha = PVR_ALPHA_DISABLE;
    }
    else {
        dst->gen.alpha = PVR_ALPHA_ENABLE;
    }

	dst->blend.src = blend_src;
	dst->blend.dst = blend_dst;

    dst->blend.src_enable = PVR_BLEND_DISABLE;
    dst->blend.dst_enable = PVR_BLEND_DISABLE;
    dst->gen.fog_type = gen_fog_type;
    dst->gen.color_clamp = PVR_CLRCLAMP_DISABLE;

	pvr_poly_compile_fast(hdr, dst);
}

/* Create a textured polygon context with parameters similar to
   the old "ta" function `ta_poly_hdr_txr' */
void pvr_poly_cxt_txr_fast(pvr_poly_hdr_t *hdr, pvr_list_t list,
                      int textureformat, int tw, int th, pvr_ptr_t textureaddr,
                      int filtering,
					// pvrTexAddress(&cxt, meshes[n].material->texture->getAddressU(), meshes[n].material->texture->getAddressV()),
					  int flip_u,
					  int clamp_u,
					  int flip_v,
					  int clamp_v,
						// PVR_UVFMT_16BIT,
						int fmt_uv,
				// PVR_CLRFMT_4FLOATS,
					  int fmt_color,
				// isMatFX ? PVR_BLEND_SRCALPHA : doBlend ? srcBlend : PVR_BLEND_ONE,
					int blend_src,
				// isMatFX ? PVR_BLEND_INVSRCALPHA : doBlend ? dstBlend : PVR_BLEND_ZERO,
					int blend_dst,
				// zFunction,
					int depth_comparison,
				// zWrite,
					int depth_write,
				// cullMode == CULLNONE ? PVR_CULLING_SMALL : cullMode == CULLBACK ? PVR_CULLING_CW : PVR_CULLING_CCW,
					int gen_culling,
				// fogEnabled ? PVR_FOG_TABLE : PVR_FOG_DISABLE
					int gen_fog_type
					  ) {
    int alpha;

	pvr_poly_cxt_t cxt;
	pvr_poly_cxt_t *dst = &cxt;

    /* Start off blank */
    memset(dst, 0, sizeof(pvr_poly_cxt_t));

    /* Fill in a few values */
    dst->list_type = list;
    alpha = list > PVR_LIST_OP_MOD;
    dst->fmt.color = fmt_color;
    dst->fmt.uv = fmt_uv;
    dst->gen.shading = PVR_SHADE_GOURAUD;
    dst->depth.comparison = depth_comparison;
    dst->depth.write = depth_write;
    dst->gen.culling = gen_culling;
    dst->txr.enable = PVR_TEXTURE_ENABLE;

    if(!alpha) {
        dst->gen.alpha = PVR_ALPHA_DISABLE;
        dst->txr.alpha = PVR_TXRALPHA_ENABLE;
        dst->txr.env = PVR_TXRENV_MODULATE;
    }
    else {
        dst->gen.alpha = PVR_ALPHA_ENABLE;
        dst->txr.alpha = PVR_TXRALPHA_ENABLE;
        dst->txr.env = PVR_TXRENV_MODULATEALPHA;
    }

	dst->blend.src = blend_src;
	dst->blend.dst = blend_dst;

    dst->blend.src_enable = PVR_BLEND_DISABLE;
    dst->blend.dst_enable = PVR_BLEND_DISABLE;
    dst->gen.fog_type = gen_fog_type;
    dst->gen.color_clamp = PVR_CLRCLAMP_DISABLE;
    dst->txr.uv_flip = flip_u | flip_v;
    dst->txr.uv_clamp = clamp_u | clamp_v;
    dst->txr.filter = filtering;
    dst->txr.mipmap_bias = PVR_MIPBIAS_NORMAL;
    dst->txr.width = tw;
    dst->txr.height = th;
    dst->txr.base = textureaddr;
    dst->txr.format = textureformat;

	pvr_poly_compile_fast(hdr, dst);
}


template<bool small_xyz, unsigned forClip>
__attribute__ ((noinline)) void tnlMeshletTransform(uint8_t* dst, const uint8_t* vertexData, uint32_t vertexCount, uint32_t vertexSize) {
	const uint8_t* next_vertex = vertexData;
	dcache_pref_block(vertexData);

	pvr_vertex64_t *sq  = (pvr_vertex64_t *)dst;

	do {
		auto vertex = next_vertex;
		next_vertex += vertexSize;

		float x, y, z, w;

		dcache_pref_block(vertex + 32);

		if (!small_xyz) {
			auto stripVert = reinterpret_cast<const V3d *>(vertex);

			mat_trans_nodiv_nomod(stripVert->x, stripVert->y, stripVert->z, x, y, z, w);

		} else {
			auto stripVert = reinterpret_cast<const int16_t *>(vertex);

			mat_trans_nodiv_nomod((float)stripVert[0], (float)stripVert[1], (float)stripVert[2], 
								x, y, z, w);
		}


		if (forClip) {
			sq->o_a = x;
			sq->o_r = y;
			sq->o_g = w;	
		}

		if (forClip == 1) { // not textured
			sq->o_b = z;
		} else if (forClip == 2) { // textured
			sq->tex_z = z;
		}

		w = frsqrt(w * w);

		sq->x = x * w;
		sq->y = y * w;
		sq->z = w;

		sq += 1;
	} while(--vertexCount != 0);
}

#if defined(DC_SH4)
template<>
__attribute__ ((noinline)) void tnlMeshletTransform<true, 0>(uint8_t* dst, const uint8_t* vertexData, uint32_t vertexCount, uint32_t vertexSize) {
	// small_xyz = true, forClip = false
	// %0 is dst (r4)
	// %1 is vertexData (r5)
	// %2 is vertexCount (r6)
	// %3 is vertexSize (r7)


	// xmtrx is already loaded into the FPU

#if 1 
	// easy, one thing at a time version
	// in the future, we want to have a few different vertices in flight to better mask
	// ftrv, fmul, fssra, fmul fmul stalls
	__asm__ __volatile__ (
		R"(
			pref @%[vtxData]

			add #16, %[dst]				! %[dst] is 12 bytes ahead, as we write back-to-front, also +4 to skip pcw

			mov %[vtxData], r0		! r0 is for pref
			add #32, r0				! mov has 0 cycle latency

			pref @r0				! prefetch next cache line, r0 is free after this
			mov %[vtxData], r1		! r1 is current_vertex

			mov.w @r1,r0			! read x
			add %[vtxSz], %[vtxData]		! advance next_vertex by vertex_size

			lds	r0,fpul				! load x to fpul
			mov.w @(2, r1),r0		! read y, not sure this will dual issue

			float fpul,fr0			! convert x to float
			lds	r0,fpul				! load y to fpul, not sure this will dual issue

			mov.w @(4, r1),r0		! read z vertex, r1 is free after this
			float	fpul,fr1		! convert y to float

			lds	r0,fpul				! load z to fpul
			! nop

			float	fpul,fr2		! convert z to float
			fldi1	fr3		    	! load 1.0f to fr3
			
			.align 2
			1:

				! we might get some stalls with ftrv here
				ftrv	xmtrx,fv0		! transform
				dt %[vtxCnt]

				mov %[vtxData], r0		! r0 is for pref
				add #32, r0				! mov has 0 cycle latency

				pref @r0				! prefetch next cache line, r0 is free after this
				mov %[vtxData], r1		! r1 is current_vertex

				mov.w @r1,r0			! read x
				add %[vtxSz], %[vtxData]		! advance next_vertex by vertex_size

				fmul	fr3, fr3		! w = w * w
				! nop

				lds	r0,fpul				! load x to fpul
				mov.w @(2, r1),r0		! read y, not sure this will dual issue

				float fpul,fr4			! convert x to float
				lds	r0,fpul				! load y to fpul, not sure this will dual issue

				fsrra	fr3				! w = 1.0f / sqrt(w)
				! nop

				mov.w @(4, r1),r0		! read z vertex, r1 is free after this
				float	fpul,fr5		! convert y to float

				lds	r0,fpul				! load z to fpul
				! nop

				fmul	fr3, fr1		! y = y * w
				fmov.s fr3, @-%[dst]	! write w

				fmul	fr3, fr0		! x = x * w
				! nop

				float	fpul,fr2		! convert z to float
				fldi1	fr3		    	! load 1.0f to fr3

				fmov.s fr1, @-%[dst]	! write y
				! nop

				fmov fr5, fr1
				! nop

				fmov.s fr0, @-%[dst]	! write x
				! nop

				fmov fr4, fr0
				! nop

				bf.s 1b
				add #76, %[dst]			! increment %0 by 64+12=76 bytes ahead, as we write back-to-front
		)"
		: [dst] "+r" (dst), [vtxData] "+r" (vertexData), [vtxCnt] "+r" (vertexCount), [vtxSz] "+r" (vertexSize) 
		:
		: "r0", "r1", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "memory"
	);
#endif
}
#endif

float16 decode_compessed_uv[256] = {
	-128 / 127.f, -127 / 127.f, -126 / 127.f, -125 / 127.f, -124 / 127.f, -123 / 127.f, -122 / 127.f, -121 / 127.f, -120 / 127.f, -119 / 127.f, -118 / 127.f, -117 / 127.f, -116 / 127.f, -115 / 127.f, -114 / 127.f, -113 / 127.f,
	-112 / 127.f, -111 / 127.f, -110 / 127.f, -109 / 127.f, -108 / 127.f, -107 / 127.f, -106 / 127.f, -105 / 127.f, -104 / 127.f, -103 / 127.f, -102 / 127.f, -101 / 127.f, -100 / 127.f, -99 / 127.f, -98 / 127.f, -97 / 127.f,
	-96 / 127.f, -95 / 127.f, -94 / 127.f, -93 / 127.f, -92 / 127.f, -91 / 127.f, -90 / 127.f, -89 / 127.f, -88 / 127.f, -87 / 127.f, -86 / 127.f, -85 / 127.f, -84 / 127.f, -83 / 127.f, -82 / 127.f, -81 / 127.f,
	-80 / 127.f, -79 / 127.f, -78 / 127.f, -77 / 127.f, -76 / 127.f, -75 / 127.f, -74 / 127.f, -73 / 127.f, -72 / 127.f, -71 / 127.f, -70 / 127.f, -69 / 127.f, -68 / 127.f, -67 / 127.f, -66 / 127.f, -65 / 127.f,
	-64 / 127.f, -63 / 127.f, -62 / 127.f, -61 / 127.f, -60 / 127.f, -59 / 127.f, -58 / 127.f, -57 / 127.f, -56 / 127.f, -55 / 127.f, -54 / 127.f, -53 / 127.f, -52 / 127.f, -51 / 127.f, -50 / 127.f, -49 / 127.f,
	-48 / 127.f, -47 / 127.f, -46 / 127.f, -45 / 127.f, -44 / 127.f, -43 / 127.f, -42 / 127.f, -41 / 127.f, -40 / 127.f, -39 / 127.f, -38 / 127.f, -37 / 127.f, -36 / 127.f, -35 / 127.f, -34 / 127.f, -33 / 127.f,
	-32 / 127.f, -31 / 127.f, -30 / 127.f, -29 / 127.f, -28 / 127.f, -27 / 127.f, -26 / 127.f, -25 / 127.f, -24 / 127.f, -23 / 127.f, -22 / 127.f, -21 / 127.f, -20 / 127.f, -19 / 127.f, -18 / 127.f, -17 / 127.f,
	-16 / 127.f, -15 / 127.f, -14 / 127.f, -13 / 127.f, -12 / 127.f, -11 / 127.f, -10 / 127.f, -9 / 127.f, -8 / 127.f, -7 / 127.f, -6 / 127.f, -5 / 127.f, -4 / 127.f, -3 / 127.f, -2 / 127.f, -1 / 127.f,

	0 / 127.f, 1 / 127.f, 2 / 127.f, 3 / 127.f, 4 / 127.f, 5 / 127.f, 6 / 127.f, 7 / 127.f, 8 / 127.f, 9 / 127.f, 10 / 127.f, 11 / 127.f, 12 / 127.f, 13 / 127.f, 14 / 127.f, 15 / 127.f,
	16 / 127.f, 17 / 127.f, 18 / 127.f, 19 / 127.f, 20 / 127.f, 21 / 127.f, 22 / 127.f, 23 / 127.f, 24 / 127.f, 25 / 127.f, 26 / 127.f, 27 / 127.f, 28 / 127.f, 29 / 127.f, 30 / 127.f, 31 / 127.f,
	32 / 127.f, 33 / 127.f, 34 / 127.f, 35 / 127.f, 36 / 127.f, 37 / 127.f, 38 / 127.f, 39 / 127.f, 40 / 127.f, 41 / 127.f, 42 / 127.f, 43 / 127.f, 44 / 127.f, 45 / 127.f, 46 / 127.f, 47 / 127.f,
	48 / 127.f, 49 / 127.f, 50 / 127.f, 51 / 127.f, 52 / 127.f, 53 / 127.f, 54 / 127.f, 55 / 127.f, 56 / 127.f, 57 / 127.f, 58 / 127.f, 59 / 127.f, 60 / 127.f, 61 / 127.f, 62 / 127.f, 63 / 127.f,
	64 / 127.f, 65 / 127.f, 66 / 127.f, 67 / 127.f, 68 / 127.f, 69 / 127.f, 70 / 127.f, 71 / 127.f, 72 / 127.f, 73 / 127.f, 74 / 127.f, 75 / 127.f, 76 / 127.f, 77 / 127.f, 78 / 127.f, 79 / 127.f,
	80 / 127.f, 81 / 127.f, 82 / 127.f, 83 / 127.f, 84 / 127.f, 85 / 127.f, 86 / 127.f, 87 / 127.f, 88 / 127.f, 89 / 127.f, 90 / 127.f, 91 / 127.f, 92 / 127.f, 93 / 127.f, 94 / 127.f, 95 / 127.f,
	96 / 127.f, 97 / 127.f, 98 / 127.f, 99 / 127.f, 100 / 127.f, 101 / 127.f, 102 / 127.f, 103 / 127.f, 104 / 127.f, 105 / 127.f, 106 / 127.f, 107 / 127.f, 108 / 127.f, 109 / 127.f, 110 / 127.f, 111 / 127.f,
	112 / 127.f, 113 / 127.f, 114 / 127.f, 115 / 127.f, 116 / 127.f, 117 / 127.f, 118 / 127.f, 119 / 127.f, 120 / 127.f, 121 / 127.f, 122 / 127.f, 123 / 127.f, 124 / 127.f, 125 / 127.f, 126 / 127.f, 127 / 127.f
};

// decompressing via table may kick out some source verts from cache, but we can't do much about it
// it is still faster
template<bool small_uv>
__attribute__ ((noinline)) void tnlMeshletCopyUVs(uint8_t* dst, const uint8_t* uvData, uint32_t vertexCount, uint32_t vertexSize) {
	const uint8_t* next_vertex = uvData;

	pvr_vertex64_t *sq  = (pvr_vertex64_t *)dst;
	auto decompress_table = decode_compessed_uv + 128; // bias so we can do signed loads

	do {
		auto vertex = next_vertex;
		next_vertex += vertexSize;

		if (!small_uv) {
			sq->uv = *(const uint32_t*)vertex;
		} else {
			sq->u = decompress_table[*(const int8_t*)vertex++].raw;
			sq->v = decompress_table[*(const int8_t*)vertex].raw;
		}

		sq += 1;
	} while(--vertexCount != 0);
}

__attribute__ ((noinline)) void tnlMeshletFillResidual(uint8_t* dstCol, uint32_t vertexCount, const RGBAf *residual) {
	do {
		float *colors = (float*)dstCol;

		*colors++ = residual->alpha;
		*colors++ = residual->red;
		*colors++ = residual->green;
		*colors++ = residual->blue;

		dstCol += 64;
	} while(--vertexCount != 0);
}

#if !defined(DC_SH4)
__attribute__ ((noinline)) void tnlMeshletVertexColor(uint8_t* dstCol, const int8_t* colData, uint32_t vertexCount, uint32_t vertexSize) {
	const int8_t* next_vertex = colData;
	// should be already in cache
	// dcache_pref_block(next_vertex);

	auto vertex = next_vertex;
	next_vertex += vertexSize;
	float cB = *vertex++;
	float cG = *vertex++;
	float cR = *vertex++;
	float cA;

	vertexCount--;

	dstCol += 4 * sizeof(float);
	do {
		vertex = next_vertex;
		// should be alraedy in cache
		// dcache_pref_block(vertex + 32);

		next_vertex += vertexSize;

		float* cols = (float*)dstCol;
		
		mat_trans_nodiv_nomod(cB, cG, cR, cB, cG, cR, cA);

		*--cols = cB;
		cB = *vertex++;
		*--cols = cG;
		cG = *vertex++;
		*--cols = cR;
		cR = *vertex++;
		*--cols = cA;
		
		dstCol += 64;
	} while(--vertexCount != 0);

	float* cols = (float*)dstCol;

	mat_trans_nodiv_nomod(cB, cG, cR, cB, cG, cR, cA);

	*--cols = cB;
	*--cols = cG;
	*--cols = cR;
	*--cols = cA;
}
#else
__attribute__ ((noinline)) void tnlMeshletVertexColor(uint8_t* dstCol, const int8_t* colData, uint32_t vertexCount, uint32_t vertexSize) {
	const int8_t* next_vertex = colData;

	dstCol += 4 * sizeof(float);

	__asm__ __volatile__ (
		R"(
			mov %[next_vertex], r0
			add %[vertexSize], %[next_vertex]
			dt %[vertexCount]

			! preload B
			mov.b @r0+, r2
			lds r2, fpul
			float fpul, fr0

			! preload G
			mov.b @r0+, r2
			lds r2, fpul
			float fpul, fr1

			! preload R
			mov.b @r0+, r2
			lds r2, fpul
			float fpul, fr2

			fldi1 fr3
			
			.align 2
			1:
				ftrv xmtrx, fv0
				mov %[next_vertex], r0

				mov.b @r0+, r2
				mov %[dstCol], r1
				
				mov.b @r0+, r3
				add #64, %[dstCol]

				dt %[vertexCount]
				mov.b @r0+, r4

				lds r2, fpul
				add %[vertexSize], %[next_vertex]

				float fpul, fr4
				lds r3, fpul

				float fpul, fr5
				fmov.s fr0, @-r1

				lds r4, fpul
				! nop

				fldi1 fr7
				! nop

				fmov.s fr1, @-r1
				float fpul, fr6

				fmov.s fr2, @-r1
				bt/s 2f
				fmov.s fr3, @-r1 ! store A

				ftrv xmtrx, fv4
				mov %[next_vertex], r0

				mov.b @r0+, r2
				mov %[dstCol], r1
				
				mov.b @r0+, r3
				add #64, %[dstCol]

				dt %[vertexCount]
				mov.b @r0+, r4

				lds r2, fpul
				add %[vertexSize], %[next_vertex]

				float fpul, fr0
				lds r3, fpul

				float fpul, fr1
				fmov.s fr4, @-r1

				lds r4, fpul
				! nop

				fldi1 fr3
				! nop

				fmov.s fr5, @-r1
				float fpul, fr2

				fmov.s fr6, @-r1
				bf/s 1b
				fmov.s fr7, @-r1 ! store A
			
			! do final vertex, fv0

			ftrv xmtrx, fv0

			fmov.s fr0, @-%[dstCol]
			fmov.s fr1, @-%[dstCol]
			fmov.s fr2, @-%[dstCol]

			bra 3f
			fmov.s fr3, @-%[dstCol] ! delay slot

			2:
			! do final vertex, fv4
			ftrv xmtrx, fv4

			fmov.s fr4, @-%[dstCol]
			fmov.s fr5, @-%[dstCol]
			fmov.s fr6, @-%[dstCol]
			fmov.s fr7, @-%[dstCol]

			3:

		)"
		: [dstCol] "+r" (dstCol), [next_vertex] "+r" (next_vertex), [vertexCount] "+r" (vertexCount), [vertexSize] "+r" (vertexSize) 
		:
		: "r0", "r1", "r2", "r3", "r4", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "memory"
	);
}
#endif

__attribute__ ((noinline)) void tnlMeshletVertexColorBaked(uint8_t* dstCol, const int8_t* colData, uint32_t vertexCount) {
	const int8_t* next_vertex = colData;
	// should be already in cache
	// dcache_pref_block(next_vertex);

	auto vertex = next_vertex;
	next_vertex += 3;
	float cB = *vertex++;
	float cG = *vertex++;
	float cR = *vertex++;
	float cA;

	vertexCount--;

	dstCol += 4 * sizeof(float);
	do {
		vertex = next_vertex;
		// should be alraedy in cache
		// dcache_pref_block(vertex + 32);

		next_vertex += 3;

		float* cols = (float*)dstCol;
		
		mat_trans_nodiv_nomod(cB, cG, cR, cB, cG, cR, cA);

		*--cols = cB;
		cB = *vertex++;
		*--cols = cG;
		cG = *vertex++;
		*--cols = cR;
		cR = *vertex++;
		*--cols = cA;
		
		dstCol += 64;
	} while(--vertexCount != 0);

	float* cols = (float*)dstCol;

	mat_trans_nodiv_nomod(cB, cG, cR, cB, cG, cR, cA);

	*--cols = cB;
	*--cols = cG;
	*--cols = cR;
	*--cols = cA;
}

template <unsigned cntDiffuse, bool floatNormals>
__attribute__ ((noinline)) void tnlMeshletDiffuseColor(uint8_t* dstCol, const uint8_t* normalData, uint32_t vertexCount, uint32_t vertexSize, const RGBAf *lights) {
	
	const uint8_t* next_vertex = normalData;
	dcache_pref_block(next_vertex);

	const float light1R = lights[0].red;
	const float light1G = lights[0].green;
	const float light1B = lights[0].blue;

	const float light2R = lights[1].red;
	const float light2G = lights[1].green;
	const float light2B = lights[1].blue;

	const float light3R = lights[2].red;
	const float light3G = lights[2].green;
	const float light3B = lights[2].blue;

	const float light4R = lights[3].red;
	const float light4G = lights[3].green;
	const float light4B = lights[3].blue;

	dstCol += 1 * sizeof(float); // skip alpha, write rgb

	do {
		auto vertex = next_vertex;
		dcache_pref_block(vertex + 32);
		next_vertex += vertexSize;

		const int8_t* inxyz = (const int8_t*)vertex;

		V3d normal;
		if (!floatNormals) {
			normal = { static_cast<float>(inxyz[0]), static_cast<float>(inxyz[1]), static_cast<float>(inxyz[2])};
		} else {
			normal = *(V3d*)inxyz;
		}
		
		float light1, light2, light3, light4;
		mat_trans_nodiv_nomod_zerow(normal.x, normal.y, normal.z, light1, light2, light3, light4);

		float dR = 0, dG = 0, dB = 0;

		if (light1 > 0) {
			dR += light1 * light1R;
			dG += light1 * light1G;
			dB += light1 * light1B;
		}

		if (cntDiffuse > 1 && light2 > 0) {
			dR += light2 * light2R;
			dG += light2 * light2G;
			dB += light2 * light2B;
		}

		if (cntDiffuse > 2 && light3 > 0) {
			dR += light3 * light3R;
			dG += light3 * light3G;
			dB += light3 * light3B;
		}

		if (cntDiffuse > 3 && light4 > 0) {
			dR += light4 * light4R;
			dG += light4 * light4G;
			dB += light4 * light4B;
		}

		float *cols = (float*)dstCol;

		*cols++ += dR;
		*cols++ += dG;
		*cols++ += dB;

		dstCol += 64;
	} while(--vertexCount != 0);
}
template <bool small_xyz, bool floatNormals>
__attribute__ ((noinline)) void tnlMeshletPointColor(uint8_t* srcPos, uint8_t* dstCol, const uint8_t* normalData, uint32_t vertexCount, uint32_t vertexSize, const point_light_t* light, float det, matrix_t* ltwd, matrix_t* normalmtx, material_t* material) {
	
	const uint8_t* next_vertex = normalData;
	dcache_pref_block(next_vertex);

	V3d lightPos = light->gameObject->ltw.pos;

	const float lightR = light->color.red * light->intensity * material->color.red;
	const float lightG = light->color.green * light->intensity * material->color.green;
	const float lightB = light->color.blue * light->intensity * material->color.blue;

	const float r2 = light->Range*light->Range;
	dstCol += 1 * sizeof(float); // skip alpha, write rgb

	do {
		auto vertex = next_vertex;
		dcache_pref_block(vertex + 32);
		next_vertex += vertexSize;

		const int8_t* inxyz = (const int8_t*)vertex;


		float x, y, z, w;

		mat_load(ltwd);
		if (!small_xyz) {
			auto stripVert = reinterpret_cast<const V3d *>(srcPos);

			mat_trans_nodiv_nomod(stripVert->x, stripVert->y, stripVert->z, x, y, z, w);

		} else {
			auto stripVert = reinterpret_cast<const int16_t *>(srcPos);

			mat_trans_nodiv_nomod((float)stripVert[0], (float)stripVert[1], (float)stripVert[2], 
								x, y, z, w);
		}

		const V3d pos = { x, y, z };

		(void)w;

		V3d toL = sub(lightPos, pos);

		V3d normal;
		if (!floatNormals) {
			normal = { static_cast<float>(inxyz[0])/127, static_cast<float>(inxyz[1])/127, static_cast<float>(inxyz[2])/127};
		} else {
			normal = *(V3d*)inxyz;
		}

		mat_load(normalmtx);

		mat_trans_nodiv_nomod_zerow(normal.x, normal.y, normal.z, x, y, z, w);

		normal = { x, y, z};
		normal = normalize(normal);
		(void)w;

		#if defined(DC_SH4)
		float d2 = fipr_magnitude_sqr(toL.x, toL.y, toL.z, 0);
		#else
		float d2 = toL.x * toL.x + toL.y * toL.y + toL.z * toL.z;
		#endif

		toL = normalize(toL);

		#if defined(DC_SH4)
		float rawNdotL = fipr(normal.x, normal.y, normal.z, 0, toL.x, toL.y, toL.z, 0);
		#else
		float rawNdotL = normal.x * toL.x + normal.y * toL.y + normal.z * toL.z;
		#endif
		float invLen = 1.0f/sqrtf(d2);
		
		float lightAmt = rawNdotL;
		// float att = invLen * invLen;
		float att = 1.0f / (1.0f + 25.0f * (d2 / r2));
		lightAmt *= att / length(normal);

		if (lightAmt > 0.0001f) {

			float dR = lightAmt * lightR;
			float dG = lightAmt * lightG;
			float dB = lightAmt * lightB;

			float *cols = (float*)dstCol;

			*cols++ += dR;
			*cols++ += dG;
			*cols++ += dB;
		}

		srcPos += vertexSize;
		dstCol += 64;
	} while(--vertexCount != 0);
}

template <bool small_xyz, bool floatNormals>
__attribute__ ((noinline)) void tnlMeshletPointColorBake(uint8_t* srcPos, uint8_t* dstCol, const uint8_t* normalData, uint32_t vertexCount, uint32_t vertexSize, const point_light_t* light, matrix_t* ltwd, matrix_t* normalmtx) {
	
	const uint8_t* next_vertex = normalData;
	dcache_pref_block(next_vertex);

	V3d lightPos = light->gameObject->ltw.pos;

	const float lightR = light->color.red * light->intensity;
	const float lightG = light->color.green * light->intensity;
	const float lightB = light->color.blue * light->intensity;

	const float r2 = light->Range*light->Range;

	do {
		auto vertex = next_vertex;
		dcache_pref_block(vertex + 32);
		next_vertex += vertexSize;

		const int8_t* inxyz = (const int8_t*)vertex;


		float x, y, z, w;

		mat_load(ltwd);
		if (!small_xyz) {
			auto stripVert = reinterpret_cast<const V3d *>(srcPos);

			mat_trans_nodiv_nomod(stripVert->x, stripVert->y, stripVert->z, x, y, z, w);

		} else {
			auto stripVert = reinterpret_cast<const int16_t *>(srcPos);

			mat_trans_nodiv_nomod((float)stripVert[0], (float)stripVert[1], (float)stripVert[2], 
								x, y, z, w);
		}

		const V3d pos = { x, y, z };

		(void)w;

		V3d toL = sub(lightPos, pos);

		V3d normal;
		if (!floatNormals) {
			normal = { static_cast<float>(inxyz[0])/127, static_cast<float>(inxyz[1])/127, static_cast<float>(inxyz[2])/127};
		} else {
			normal = *(V3d*)inxyz;
		}

		mat_load(normalmtx);

		mat_trans_nodiv_nomod_zerow(normal.x, normal.y, normal.z, x, y, z, w);

		normal = { x, y, z};
		normal = normalize(normal);
		(void)w;

		#if defined(DC_SH4)
		float d2 = fipr_magnitude_sqr(toL.x, toL.y, toL.z, 0);
		#else
		float d2 = toL.x * toL.x + toL.y * toL.y + toL.z * toL.z;
		#endif

		toL = normalize(toL);

		#if defined(DC_SH4)
		float rawNdotL = fipr(normal.x, normal.y, normal.z, 0, toL.x, toL.y, toL.z, 0);
		#else
		float rawNdotL = normal.x * toL.x + normal.y * toL.y + normal.z * toL.z;
		#endif
		float invLen = 1.0f/sqrtf(d2);
		
		float lightAmt = rawNdotL;
		// float att = invLen * invLen;
		float att = 1.0f / (1.0f + 25.0f * (d2 / r2));
		lightAmt *= att / length(normal);

		if (lightAmt > 0.0001f) {

			float dR = lightAmt * lightR;
			float dG = lightAmt * lightG;
			float dB = lightAmt * lightB;

			uint8_t* cols = dstCol;

			#define COLADD(dC) do { \
				uint32_t dc = *cols; \
				dc += dC * 255; \
				if (dc > 255) dc = 255; \
				*cols = dc; \
			} while(false)
			
			COLADD(dB); cols++;
			COLADD(dG); cols++;
			COLADD(dR);

			#undef COLADD
		}

		srcPos += vertexSize;
		dstCol += 3;
	} while(--vertexCount != 0);
}

template<bool textured>
__attribute__ ((noinline)) void submitMeshlet(uint8_t* OCR, const int8_t* indexData, uint32_t indexCount) {
	struct SQBUF {
		union {
			uint32_t flags;
			uint64_t data[4];
			uint8_t data8[32];
		};
	};

	static_assert(sizeof(SQBUF) == 32);
 
	do {
		auto idx = *indexData++;
		auto flags = idx & 0x80 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
		auto lookup_idx = idx & 0x7F;

		auto src = (SQBUF*)(OCR +  lookup_idx * 64);
		src[0].flags = flags;
		FLUSH_TA_DATA(src);
		if (textured) {
			src[1].data8[31] = 0;
			FLUSH_TA_DATA(src+1);
		}
	} while(--indexCount);
}

template<bool textured>
__attribute__ ((noinline)) void submitMeshletFallback(uint8_t* OCR, const int8_t* indexData, uint32_t indexCount) {
	struct SQBUF {
		union {
			uint32_t flags;
			uint64_t data[4];
			uint8_t data8[32];
		};
	};

	SQBUF* sq = (SQBUF*)pvr_dr_target(drState);

	static_assert(sizeof(SQBUF) == 32);
 
	do {
		auto idx = *indexData++;
		auto flags = idx & 0x80 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
		auto lookup_idx = idx & 0x7F;

		auto src = (SQBUF*)(OCR +  lookup_idx * 64);
		src[0].flags = flags;
		*sq = src[0];
		pvr_dr_commit(sq);
		if (textured) {
			*sq = src[1];
			pvr_dr_commit(sq);
		}
	} while(--indexCount);
}


#if defined(DC_SH4)
template<>
__attribute__ ((noinline)) void submitMeshlet<true>(uint8_t* OCR, const int8_t* indexData, uint32_t indexCount) {

	__asm__ __volatile__ (
		R"(
			mov %[idxData], r0
			mov %[idxCnt], r1

			add #31, r1
			shlr2 r1
			shlr2 r1
			shlr r1

			.align 2
			1:
				dt r1
				pref @r0
				bf/s 1b
				add #32, r0


			mov.b @%[idxData]+, r0
			.align 2
			1:
				mov %[cmdVtx], r1

				cmp/pz r0
				and #0x7F, r0

				bt 2f
				mov %[cmdVtxEol], r1
				2:

				shll8 r0
				! nop

				shlr2 r0
				mov.b @%[idxData]+, r2

				add %[ocr], r0
				! nop

				mov.l r1, @r0

				ocbwb @r0
				! nop

				dt %[idxCnt]
				! nop

				add #32, r0
				! nop

				! nop
				! nop

				! nop
				! nop

				! nop
				! nop
				mov.w r0, @(30, r0)

				ocbwb @r0

				bf.s 1b
				mov r2, r0
		)"
		: [idxData] "+r" (indexData), [idxCnt] "+r" (indexCount)
		: [ocr] "r" (OCR), [cmdVtx] "r" (PVR_CMD_VERTEX), [cmdVtxEol] "r" (PVR_CMD_VERTEX_EOL) 
		: "r0", "r1", "r2", "memory"
	);
}
#endif


void enter_oix_() {
	#if defined(DC_SH4)
	auto mask = irq_disable();
	dcache_purge_all();
	volatile uint32_t * CCN_CCR = (uint32_t *)0xFF00001C;
	*CCN_CCR |= (1 << 7); // enable OIX
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");

	for (unsigned i = 0x92000000; i < (0x92000000 + 8192); i += 32) {
		__asm__ __volatile__ ("movca.l r0,@%0" : : "r" (i): "memory");
	}

	irq_restore(mask);
	#endif
}

void leave_oix_() {
	#if defined(DC_SH4)
	auto mask = irq_disable();
	dcache_inval_range(0x92000000, 8192);
	dcache_purge_all();
	volatile uint32_t * CCN_CCR = (uint32_t *)0xFF00001C;
	*CCN_CCR &= ~( 1 << 7); // disable OIX
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	irq_restore(mask);
	#endif
}

void enter_ocr_() {
	#if defined(DC_SH4)
	auto mask = irq_disable();
	dcache_purge_all();
	volatile uint32_t * CCN_CCR = (uint32_t *)0xFF00001C;
	*CCN_CCR |= (1 << 5); // enable OCR (ORA)
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");

	irq_restore(mask);
	#endif
}

void leave_ocr_() {
	#if defined(DC_SH4)
	auto mask = irq_disable();
	dcache_inval_range(0x92000000, 8192);
	dcache_purge_all();
	volatile uint32_t * CCN_CCR = (uint32_t *)0xFF00001C;
	*CCN_CCR &= ~( 1 << 5); // disable OCR (ORA)
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("nop");
	irq_restore(mask);
	#endif
}

#if defined(DC_SH4)
#define FLUSH_TA_DATA(src) do { __asm__ __volatile__("ocbwb @%0" : : "r" (src) : "memory"); } while(0)
#else
#define FLUSH_TA_DATA(src) do { pvr_dr_commit(src); } while(0)
#endif

#if defined(DC_SH4)
void (*enter_oix)() = (void(*)())(((uintptr_t)&enter_oix_) - 0x8c000000 + 0xAc000000);
void (*leave_oix)() = (void(*)())(((uintptr_t)&leave_oix_) - 0x8c000000 + 0xAc000000);
#else
void (*enter_oix)() = enter_oix_;
void (*leave_oix)() = leave_oix_;
#endif

// 8 kb in total
#if defined(DC_SH4)
uint8_t* OCR_SPACE;
#else
uint8_t OCR_SPACE[32 * 256] __attribute__((aligned(32)));
#endif
template<bool hasTexture>
void* interpolateAndSubmit(void* dst, const void* src1, const void* src2, uint32_t flags)  {
	auto v = (pvr_vertex64_t1 *)dst;
	auto v1 = (const pvr_vertex64_t *)src1;
	auto v2 = (const pvr_vertex64_t *)src2;

	v->flags = flags;

	// assuming near plane is 0.0f
	// v1 is visible (posi), and v2 is behind the plane (negative)
	// z is w here
	// float t = fclamp0_1((1.0f - v1->o_g) / (v2->o_g - v1->o_g));
	float SA = (hasTexture?v1->tex_z : v1->o_b) + v1->o_g;
	float SB = (hasTexture?v2->tex_z : v2->o_b) + v2->o_g;
	float t  = SA / (SA - SB);

	float x = v1->o_a + t * (v2->o_a - v1->o_a);
	float y = v1->o_r + t * (v2->o_r - v1->o_r);
	float w = v1->o_g + t * (v2->o_g - v1->o_g);

	w = frsqrt(w * w);

	v->x = x * w;
	v->y = y * w;
	v->z = w;

	if (hasTexture) {
		float16 v1_u,v1_v,v2_u,v2_v;
		v1_u.raw = v1->u;
		v1_v.raw = v1->v;

		v2_u.raw = v2->u;
		v2_v.raw = v2->v;

		float16 v_u = v1_u + t * (v2_u - v1_u);
		float16 v_v = v1_v + t * (v2_v - v1_v);

		v->u = v_u.raw;
		v->v = v_v.raw;

		pvr_dr_commit(v);
		v ++;
	}

	auto sq2 = (pvr_vertex64_t2 *)v;
	auto sq3 = (pvr_vertex32_ut *)v;

	float v1_a, v1_r, v1_g, v1_b, v2_a, v2_r, v2_g, v2_b;

	if (hasTexture) {
		auto v1t = (const pvr_vertex64_t *)v1;
		v1_a = v1t->a;
		v1_r = v1t->r;
		v1_g = v1t->g;
		v1_b = v1t->b;

		auto v2t = (const pvr_vertex64_t *)v2;
		v2_a = v2t->a;
		v2_r = v2t->r;
		v2_g = v2t->g;
		v2_b = v2t->b;
	} else {
		auto v1t = (const pvr_vertex32_ut *)v1;
		v1_a = v1t->a;
		v1_r = v1t->r;
		v1_g = v1t->g;
		v1_b = v1t->b;

		auto v2t = (const pvr_vertex32_ut *)v2;
		v2_a = v2t->a;
		v2_r = v2t->r;
		v2_g = v2t->g;
		v2_b = v2t->b;
	}

	(hasTexture ? sq2->a : sq3->a) = v1_a + t * (v2_a - v1_a);
	(hasTexture ? sq2->r : sq3->r) = v1_r + t * (v2_r - v1_r);
	(hasTexture ? sq2->g : sq3->g) = v1_g + t * (v2_g - v1_g);
	(hasTexture ? sq2->b : sq3->b) = v1_b + t * (v2_b - v1_b);

	pvr_dr_commit(hasTexture ? (void*)sq2 : (void*)sq3);

	v++;
	return v;
}

template<bool textured>
__attribute__ ((noinline)) void clipAndsubmitMeshlet(uint8_t* vertexData, const int8_t* indexData, uint32_t indexCount) {

	struct SQBUF {
		union {
			uint32_t flags;
			uint64_t data[4];
			uint8_t data8[32];
		};
	};

	static_assert(sizeof(SQBUF) == 32);

	SQBUF* sq = (SQBUF*)pvr_dr_target(drState);

	constexpr int8_t VERTEX = 0;
	constexpr int8_t VERTEX_EOL = 0x80;
	
	#define FILLVERT(n) \
		do { \
			auto idx = *indexData++; \
			auto local_idx = idx & 0x7f; \
			eol_now = idx & 0x80; \
			auto local_ptr = (vertexData + local_idx * 64); \
			vpp[n] = local_ptr; \
			auto v = (const pvr_vertex64_t*)local_ptr; \
			vismask >>= 1; \
			if((textured?v->tex_z:v->o_b) >= -v->o_g) vismask |= 0b100;	\
			indexCount--; \
			currentCount++; \
		} while(0)

	#define SUBMIT_VTX(vid, eolf) \
		do { \
			auto src = (SQBUF*) vpp[vid]; \
			src[0].flags = eolf ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX; \
			FLUSH_TA_DATA(src); \
			if (textured) { \
				src[1].data8[31] = 0; \
				FLUSH_TA_DATA(src + 1); \
			} \
		} while(0)

	#define SUBMIT_INTERPOLATE(vid1, vid2, eolf) \
		do { \
			sq = (SQBUF*)interpolateAndSubmit<textured>(sq, vpp[vid1], vpp[vid2], eolf ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX); \
		} while(0)

	uint32_t vismask = 0;

	uint8_t* vpp[3];

	int8_t eol = 0;
	int8_t eol_now = 0;

	do {
		uint32_t currentCount = -1;

		FILLVERT(0);
		FILLVERT(1);
		FILLVERT(2);

		if (vismask & 1) {
			SUBMIT_VTX(0, VERTEX);
			if (vismask & 2) {
				// both first verts visible
				SUBMIT_VTX(1, VERTEX);
			} else {
				// 0 visible, 1 hidden
				SUBMIT_INTERPOLATE(0, 1, VERTEX);
			}
		} else if (vismask & 2) {
			// 0 hidden, 1 visible
			SUBMIT_INTERPOLATE(1, 0, VERTEX);
			SUBMIT_VTX(1, VERTEX);
		}

		eol = 0;
		// each remaining vertex of the strip
		while(!eol) {
			// "ring buffery" indices
			uint8_t vertZeroIdx = (currentCount - 2) % 3;
			uint8_t vertOneIdx = (currentCount - 1) % 3;
			uint8_t vertTwoIdx = currentCount % 3;
			//dcache_pref_block(&vph[vertZeroIdx]); not sure where to put this honestly -jaxyn

			eol = eol_now;

			if (!vismask) {
				if (!eol) {
					// "ring buffery" filling
					FILLVERT(vertZeroIdx);
				}
				continue;
			}

			if (vismask == 7) {
				// all visible
				SUBMIT_VTX(vertTwoIdx, eol);
				if (!eol) {
					// "ring buffery" filling
					FILLVERT(vertZeroIdx);
				}
				continue;
			}

			switch (vismask) {
				case 1: // 0 visible, 1 and 2 hidden
					// pause strip
					SUBMIT_INTERPOLATE(vertZeroIdx, vertTwoIdx, VERTEX_EOL);
					break;
				case 3: // 0 and 1 visible, 2 hidden
					SUBMIT_INTERPOLATE(vertZeroIdx, vertTwoIdx, VERTEX);
					SUBMIT_VTX(vertOneIdx, VERTEX);
				case 2: // 0 hidden, 1 visible, 2 hidden
					SUBMIT_INTERPOLATE(vertOneIdx, vertTwoIdx, eol);
					break;
				case 4: // 0 and 1 hidden, 2 visible
					SUBMIT_INTERPOLATE(vertTwoIdx, vertZeroIdx, VERTEX);
					if (currentCount & 0x01) { // flip directionality
				case 5: // 0 visible, 1 hidden, 2 visible
						SUBMIT_VTX(vertTwoIdx, VERTEX);
					}
					SUBMIT_INTERPOLATE(vertTwoIdx, vertOneIdx, VERTEX);
					SUBMIT_VTX(vertTwoIdx, eol);
					break;
				case 6: // 0 hidden, 1 and 2 visible
					SUBMIT_INTERPOLATE(vertTwoIdx, vertZeroIdx, VERTEX);
					SUBMIT_VTX(vertOneIdx, VERTEX);
					SUBMIT_VTX(vertTwoIdx, eol);
					break;
				default:
					break;
			}

			if (!eol) {
				// "ring buffery" filling
				FILLVERT(vertZeroIdx);
			}
		};
	} while(indexCount != 0);

	#undef FILLVERT
	#undef SUBMIT_VTX
	#undef SUBMIT_INTERPOLATE
}


template<bool textured>
__attribute__ ((noinline)) void clipAndsubmitMeshletFallback(uint8_t* vertexData, const int8_t* indexData, uint32_t indexCount) {

	struct SQBUF {
		union {
			uint32_t flags;
			uint64_t data[4];
			uint8_t data8[32];
		};
	};

	static_assert(sizeof(SQBUF) == 32);

	SQBUF* sq = (SQBUF*)pvr_dr_target(drState);

	constexpr int8_t VERTEX = 0;
	constexpr int8_t VERTEX_EOL = 0x80;
	
	#define FILLVERT(n) \
		do { \
			auto idx = *indexData++; \
			auto local_idx = idx & 0x7f; \
			eol_now = idx & 0x80; \
			auto local_ptr = (vertexData + local_idx * 64); \
			vpp[n] = local_ptr; \
			auto v = (const pvr_vertex64_t*)local_ptr; \
			vismask >>= 1; \
			if((textured?v->tex_z:v->o_b) >= -v->o_g) vismask |= 0b100;	\
			indexCount--; \
			currentCount++; \
		} while(0)

	#define SUBMIT_VTX(vid, eolf) \
		do { \
			auto src = (SQBUF*) vpp[vid]; \
			src[0].flags = eolf ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX; \
			*sq = src[0]; \
			pvr_dr_commit(sq); \
			if (textured) { \
				*sq = src[1]; \
				pvr_dr_commit(sq); \
			} \
		} while(0)

	#define SUBMIT_INTERPOLATE(vid1, vid2, eolf) \
		do { \
			sq = (SQBUF*)interpolateAndSubmit<textured>(sq, vpp[vid1], vpp[vid2], eolf ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX); \
		} while(0)

	uint32_t vismask = 0;

	uint8_t* vpp[3];

	int8_t eol = 0;
	int8_t eol_now = 0;

	do {
		uint32_t currentCount = -1;

		FILLVERT(0);
		FILLVERT(1);
		FILLVERT(2);

		if (vismask & 1) {
			SUBMIT_VTX(0, VERTEX);
			if (vismask & 2) {
				// both first verts visible
				SUBMIT_VTX(1, VERTEX);
			} else {
				// 0 visible, 1 hidden
				SUBMIT_INTERPOLATE(0, 1, VERTEX);
			}
		} else if (vismask & 2) {
			// 0 hidden, 1 visible
			SUBMIT_INTERPOLATE(1, 0, VERTEX);
			SUBMIT_VTX(1, VERTEX);
		}

		eol = 0;
		// each remaining vertex of the strip
		while(!eol) {
			// "ring buffery" indices
			uint8_t vertZeroIdx = (currentCount - 2) % 3;
			uint8_t vertOneIdx = (currentCount - 1) % 3;
			uint8_t vertTwoIdx = currentCount % 3;
			//dcache_pref_block(&vph[vertZeroIdx]); not sure where to put this honestly -jaxyn

			eol = eol_now;

			if (!vismask) {
				if (!eol) {
					// "ring buffery" filling
					FILLVERT(vertZeroIdx);
				}
				continue;
			}

			if (vismask == 7) {
				// all visible
				SUBMIT_VTX(vertTwoIdx, eol);
				if (!eol) {
					// "ring buffery" filling
					FILLVERT(vertZeroIdx);
				}
				continue;
			}

			switch (vismask) {
				case 1: // 0 visible, 1 and 2 hidden
					// pause strip
					SUBMIT_INTERPOLATE(vertZeroIdx, vertTwoIdx, VERTEX_EOL);
					break;
				case 3: // 0 and 1 visible, 2 hidden
					SUBMIT_INTERPOLATE(vertZeroIdx, vertTwoIdx, VERTEX);
					SUBMIT_VTX(vertOneIdx, VERTEX);
				case 2: // 0 hidden, 1 visible, 2 hidden
					SUBMIT_INTERPOLATE(vertOneIdx, vertTwoIdx, eol);
					break;
				case 4: // 0 and 1 hidden, 2 visible
					SUBMIT_INTERPOLATE(vertTwoIdx, vertZeroIdx, VERTEX);
					if (currentCount & 0x01) { // flip directionality
				case 5: // 0 visible, 1 hidden, 2 visible
						SUBMIT_VTX(vertTwoIdx, VERTEX);
					}
					SUBMIT_INTERPOLATE(vertTwoIdx, vertOneIdx, VERTEX);
					SUBMIT_VTX(vertTwoIdx, eol);
					break;
				case 6: // 0 hidden, 1 and 2 visible
					SUBMIT_INTERPOLATE(vertTwoIdx, vertZeroIdx, VERTEX);
					SUBMIT_VTX(vertOneIdx, VERTEX);
					SUBMIT_VTX(vertTwoIdx, eol);
					break;
				default:
					break;
			}

			if (!eol) {
				// "ring buffery" filling
				FILLVERT(vertZeroIdx);
			}
		};
	} while(indexCount != 0);

	#undef FILLVERT
	#undef SUBMIT_VTX
	#undef SUBMIT_INTERPOLATE
}

static constexpr void (*tnlMeshletTransformSelector[6])(uint8_t* dst, const uint8_t* vertexData, uint32_t vertexCount, uint32_t vertexSize) {
	&tnlMeshletTransform<false, 0>,
	&tnlMeshletTransform<true , 0>,
	&tnlMeshletTransform<false, 1 >,
	&tnlMeshletTransform<true , 1 >,
	&tnlMeshletTransform<false, 2 >,
	&tnlMeshletTransform<true , 2 >,
};

static constexpr void (*tnlMeshletCopyUVsSelector[2])(uint8_t* dst, const uint8_t* uvData, uint32_t vertexCount, uint32_t vertexSize) = {
	&tnlMeshletCopyUVs<false>,
	&tnlMeshletCopyUVs<true>
};

static constexpr void (*tnlMeshletFillResidualSelector[1])(uint8_t* dstCol, uint32_t vertexCount, const RGBAf *residual) = {
	&tnlMeshletFillResidual,
};

static constexpr void (*tnlMeshletVertexColorSelector[1])(uint8_t* dstCol, const int8_t* colData, uint32_t vertexCount, uint32_t vertexSize) = {
	&tnlMeshletVertexColor,
};

static constexpr void (*tnlMeshletVertexColorBakedSelector[1])(uint8_t* dstCol, const int8_t* colData, uint32_t vertexCount) = {
	&tnlMeshletVertexColorBaked,
};

//tnlMeshletDiffuseColor
static constexpr void (*tnlMeshletDiffuseColorSelector[8])(uint8_t* dstCol, const uint8_t* normalData, uint32_t vertexCount, uint32_t vertexSize, const RGBAf *lights) = {
	&tnlMeshletDiffuseColor<1, false>,
	&tnlMeshletDiffuseColor<2, false>,
	&tnlMeshletDiffuseColor<3, false>,
	&tnlMeshletDiffuseColor<4, false>,

	&tnlMeshletDiffuseColor<1, true>,
	&tnlMeshletDiffuseColor<2, true>,
	&tnlMeshletDiffuseColor<3, true>,
	&tnlMeshletDiffuseColor<4, true>,
};

static constexpr void (*tnlMeshletPointColorSelector[8])(uint8_t* srcPos, uint8_t* dstCol, const uint8_t* normalData, uint32_t vertexCount, uint32_t vertexSize, const point_light_t *light, float det, matrix_t* ltwd, matrix_t* normalmtx, material_t* material) = {
	&tnlMeshletPointColor<false, false>,
	&tnlMeshletPointColor<true, false>,
};

static constexpr void (*tnlMeshletPointColorBakeSelector[8])(uint8_t* srcPos, uint8_t* dstCol, const uint8_t* normalData, uint32_t vertexCount, uint32_t vertexSize, const point_light_t *light, matrix_t* ltwd, matrix_t* normalmtx) = {
	&tnlMeshletPointColorBake<false, false>,
	&tnlMeshletPointColorBake<true, false>,
};

static void (*submitMeshletSelector[2])(uint8_t* OCR, const int8_t* indexData, uint32_t indexCount) = {
	&submitMeshlet<false>,
	&submitMeshlet<true>,
};

static void (*submitMeshletSelectorFallback[2])(uint8_t* OCR, const int8_t* indexData, uint32_t indexCount) = {
	&submitMeshletFallback<false>,
	&submitMeshletFallback<true>,
};

static void (*clipAndsubmitMeshletSelector[2])(uint8_t* OCR, const int8_t* indexData, uint32_t indexCount) = {
	&clipAndsubmitMeshlet<false>,
	&clipAndsubmitMeshlet<true>,
};

static void (*clipAndsubmitMeshletSelectorFallback[2])(uint8_t* OCR, const int8_t* indexData, uint32_t indexCount) = {
	&clipAndsubmitMeshletFallback<false>,
	&clipAndsubmitMeshletFallback<true>,
};

RGBAf ambLight = { 0.7f, 0.7f, 0.7f, 1.0f };
float ambient = 0.2f;

size_t vertexBufferFree() {
    size_t end   = PVR_GET(PVR_TA_VERTBUF_END);
    size_t pos   = PVR_GET(PVR_TA_VERTBUF_POS);

    size_t free  = end - pos;

	return free;
}

bool vertexOverflown() {
	return  PVR_GET(PVR_TA_VERTBUF_POS) >= PVR_GET(PVR_TA_VERTBUF_END) ||
			(PVR_GET(PVR_TA_OPB_POS)*4 >= PVR_GET(PVR_TA_OPB_END) && PVR_GET(PVR_TA_OPB_POS) != PVR_GET(PVR_TA_OPB_INIT));
}

constexpr size_t freeVertexTarget_Step_Up = 32 * 1024;
constexpr size_t freeVertexTarget_Step_Down = 4 * 1024;
constexpr size_t freeVertexTarget_Min = 64 * 1024;
size_t freeVertexTarget = freeVertexTarget_Min;

struct alignas(8) UniformObject
{
	float dir[MAX_LIGHTS/4][4][4];
	RGBAf col[MAX_LIGHTS];
	RGBAf        ambLight;
	int lightCount;
};

float GetMaxScale(const r_matrix_t& mat) {
    // Compute the scale factors for each axis
    float scaleRight = mat.right.x * mat.right.x + mat.right.y * mat.right.y + mat.right.z * mat.right.z;

    float scaleUp = mat.up.x * mat.up.x + mat.up.y * mat.up.y + mat.up.z * mat.up.z;

    float scaleAt = mat.at.x * mat.at.x + mat.at.y * mat.at.y + mat.at.z * mat.at.z;

    // Return the maximum scale factor
    return std::sqrt(std::max({scaleRight, scaleUp, scaleAt}));
}

#if defined(DEBUG_LOOKAT)
game_object_t* pointedGameObject = nullptr;
#endif

static float zBuffer[32][32];
#if defined(DC_SIM)
unsigned total_idx;
#endif

bool hasPointLights = false;

template<int list>
void renderMesh(camera_t* cam, game_object_t* go) {

	bool isTransp = list != PVR_LIST_OP_POLY;

    if (vertexBufferFree() < freeVertexTarget) {
        return;
    }
    bool global_needsNoClip = false;

    auto global_visible = cam->frustumTestSphereNear(&go->meshSphere);
    if (global_visible == camera_t::SPHEREOUTSIDE) {
        // printf("Outside frustum cull (%f, %f, %f) %f\n", sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius);
        return;
    } else if (global_visible == camera_t::SPHEREINSIDE) {
        global_needsNoClip = true;
    } else {
        // printf("Needs local clip (%f, %f, %f) %f\n", sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius);
    }

	{
		mat_load(&cam->devViewProjScreen);
		mat_apply((matrix_t*)&go->ltw);
		float cx = go->mesh->bounding_sphere.center.x;
		float cy = go->mesh->bounding_sphere.center.y;
		float cz = go->mesh->bounding_sphere.center.z;
		float r  = go->mesh->bounding_sphere.radius;

		float v[3 * 8] = {
			// bottom face (z = cz - r)
			cx - r, cy - r, cz - r,  // corner 0
			cx + r, cy - r, cz - r,  // corner 1
			cx + r, cy + r, cz - r,  // corner 2
			cx - r, cy + r, cz - r,  // corner 3

			// top face   (z = cz + r)
			cx - r, cy - r, cz + r,  // corner 4
			cx + r, cy - r, cz + r,  // corner 5
			cx + r, cy + r, cz + r,  // corner 6
			cx - r, cy + r, cz + r   // corner 7
		};

		for (unsigned i = 0; i < 8; i++) {
			float z;
			mat_trans_nodiv_nomod(v[i*3 + 0], v[i*3 + 1], v[i*3 + 2], v[i*3 + 0], v[i*3 + 1], z, v[i*3 + 2]);
			(void)z;
			if (v[i*3 + 0] <= 0) {
				goto skip_test; // needs clipping let's just skip for now
			}

			v[i*3 + 2] = 1/v[i*3 + 2];
			v[i*3 + 0] *= v[i*3 + 2];
			v[i*3 + 1] *= v[i*3 + 2];
		}

		float minX =  FLT_MAX;
		float maxX = -FLT_MAX;
		float minY =  FLT_MAX;
		float maxY = -FLT_MAX;
		float maxZ = -FLT_MAX;

		for (int i = 0; i < 8; ++i) {
			float x = v[i*3 + 0];
			float y = v[i*3 + 1];
			float z = v[i*3 + 2];

			if (x < minX) minX = x;
			if (x > maxX) maxX = x;
			if (y < minY) minY = y;
			if (y > maxY) maxY = y;
			if (z > maxZ) maxZ = z;
		}

		if (minX < 0) minX = 0;
		if (minX > 639) minX = 639;
		if (minY < 0) minY = 0;
		if (minY > 479) minY = 479;
		if (maxX < 0) maxX = 0;
		if (maxX > 639) maxX = 639;
		if (maxY < 0) maxY = 0;
		if (maxY > 479) maxY = 479;

		if (minX >= maxX || minY >= maxY) {
			// std::cout << "Not visible in screen space" << std::endl;
		}

		unsigned iMinX = minX/20.f;
		unsigned iMaxX = (maxX + 19.99f)/20.f;
		if (iMaxX == 32) { iMaxX--; }

		unsigned iMinY = minY/16.f;
		unsigned iMaxY = (maxY + 15.99f)/16.f;
		if (iMaxY == 32) { iMaxY--; }

		#if defined(DC_SIM)
		assert(iMinX>=0 && iMaxX<=32);
		assert(iMinY>=0 && iMaxY<=32);
		#endif

		// std::cout << "RECT: " << iMinX << ", "<< iMinY << " ~ " << iMaxX << ", " << iMaxY << " z=" << maxZ << std::endl;

		for (unsigned y = iMinY; y <= iMaxY; y++) {
			for (unsigned x = iMinX; x <= iMaxX; x++) {
				if (zBuffer[y][x] < maxZ) {
					goto skip_test; // failed test, skip the rest of it
				}
			}
		}
		
		// std::cout << " occluded" << std::endl;
		return;
	}
	skip_test:
	

    unsigned cntDiffuse;
    r_matrix_t invLtw;
	float det;
    invertGeneral(&invLtw, &det, &go->ltw);

	unsigned culling = det > 0 ? PVR_CULLING_CCW : PVR_CULLING_CW;

    UniformObject uniformObject;
    mat_load((matrix_t*)&invLtw);
	{
		unsigned n = 0;
		for (auto directional = directional_lights; *directional; directional++)
		{
			uniformObject.col[n] = (*directional)->color;
			uniformObject.col[n].alpha *= (*directional)->intensity;
			uniformObject.col[n].red *= (*directional)->intensity;
			uniformObject.col[n].green *= (*directional)->intensity;
			uniformObject.col[n].blue *= (*directional)->intensity;
			mat_trans_nodiv_nomod_zerow(
				-(*directional)->gameObject->ltw.at.x, -(*directional)->gameObject->ltw.at.y, -(*directional)->gameObject->ltw.at.z,
				uniformObject.dir[n>>2][0][n&3],
				uniformObject.dir[n>>2][1][n&3],
				uniformObject.dir[n>>2][2][n&3],
				uniformObject.dir[n>>2][3][n&3]
			);

			uniformObject.dir[n>>2][3][n&3] = 0;
			if (++n == 8)
				break;
		}

		cntDiffuse = n;
	}

    const MeshInfo* meshInfo = (const MeshInfo*)&go->mesh->data[0];

    for (size_t submesh_num = 0; submesh_num < go->submesh_count; submesh_num++) {
		if (go->materials[submesh_num]->hasAlpha() != isTransp) {
			continue;
		}
        pvr_poly_hdr_t hdr;
        bool textured = go->materials[submesh_num]->texture != nullptr;

        if (go->materials[submesh_num]->texture) {
            pvr_poly_cxt_txr_fast(
                &hdr,
                list,

                go->materials[submesh_num]->texture->flags,
                go->materials[submesh_num]->texture->lw,
                go->materials[submesh_num]->texture->lh,
                (uint8_t*)go->materials[submesh_num]->texture->data - go->materials[submesh_num]->texture->offs,

                PVR_FILTER_BILINEAR,

                // flip_u, clamp_u, flip_v, clamp_v,
                PVR_UVFLIP_NONE,
                PVR_UVCLAMP_NONE,
                PVR_UVFLIP_NONE,
                PVR_UVCLAMP_NONE,
                PVR_UVFMT_16BIT,

                PVR_CLRFMT_4FLOATS,
                list != PVR_LIST_OP_POLY ? PVR_BLEND_SRCALPHA : PVR_BLEND_ONE,
                list != PVR_LIST_OP_POLY ? PVR_BLEND_INVSRCALPHA : PVR_BLEND_ZERO,
                PVR_DEPTHCMP_GEQUAL,
                PVR_DEPTHWRITE_ENABLE,
                culling,
                PVR_FOG_DISABLE
            );
        } else {
            pvr_poly_cxt_col_fast(
                &hdr,
                list,

                PVR_CLRFMT_4FLOATS,
                list != PVR_LIST_OP_POLY ? PVR_BLEND_SRCALPHA : PVR_BLEND_ONE,
                list != PVR_LIST_OP_POLY ? PVR_BLEND_INVSRCALPHA : PVR_BLEND_ZERO,
                PVR_DEPTHCMP_GEQUAL,
                PVR_DEPTHWRITE_ENABLE,
                culling,
                PVR_FOG_DISABLE
            );
        }

        pvr_prim(&hdr, sizeof(hdr));

        RGBAf residual, material;
        // Ambient Alpha ALWAYS = 1.0
        residual.alpha = go->materials[submesh_num]->color.alpha;
        residual.red = ambLight.red * ambient * go->materials[submesh_num]->color.red + go->materials[submesh_num]->emission.red;
        residual.green = ambLight.green * ambient * go->materials[submesh_num]->color.green + go->materials[submesh_num]->emission.green;
        residual.blue = ambLight.blue * ambient * go->materials[submesh_num]->color.blue +  + go->materials[submesh_num]->emission.blue;
        material.alpha = go->materials[submesh_num]->color.alpha;
        material.red = (1.0f / 255.0f) * go->materials[submesh_num]->color.red;
        material.green = (1.0f / 255.0f) * go->materials[submesh_num]->color.green;
        material.blue = (1.0f / 255.0f) * go->materials[submesh_num]->color.blue;

		#if defined(DEBUG_LOOKAT)
		if (pointedGameObject == go) {
			residual.red = ambLight.red * ambient * 1;
			residual.green = ambLight.green * ambient * 0;
			residual.blue = ambLight.blue * ambient * 0;
			material.red = (1.0f / 255.0f) * 1;
			material.green = (1.0f / 255.0f) * 0;
			material.blue = (1.0f / 255.0f) * 0;
		}
		#endif


        RGBAf lightDiffuseColors[MAX_LIGHTS];

		float diffuse = 1.f; // should be based on material?
        for (unsigned i = 0; i < cntDiffuse; i++) {
            lightDiffuseColors[i].red = material.red * uniformObject.col[i].red * diffuse;
            lightDiffuseColors[i].green = material.green * uniformObject.col[i].green * diffuse;
            lightDiffuseColors[i].blue = material.blue * uniformObject.col[i].blue * diffuse;
        }

		int effectiveSubmeshNum = submesh_num;
		if (submesh_num == 0 && go->logical_submesh != -1) {
			effectiveSubmeshNum = go->submesh_count + go->logical_submesh;
		}

        auto meshletInfoBytes = &go->mesh->data[meshInfo[effectiveSubmeshNum].meshletOffset];
        
		size_t colorsSize = 0;
        for (int16_t meshletNum = 0; meshletNum < meshInfo[effectiveSubmeshNum].meshletCount; meshletNum++) {
            auto meshlet = (const MeshletInfo*)meshletInfoBytes;
            meshletInfoBytes += sizeof(MeshletInfo) - 8 ; // (skin ? 0 : 8);

			auto colorsBase = colorsSize;
			colorsSize += meshlet->vertexCount * 3;

			#if defined(DC_SIM)
			total_idx += meshlet->indexCount;
			#endif

            unsigned clippingRequired = 0;
			Sphere sphere = meshlet->boundingSphere;
			mat_load((matrix_t*)&go->ltw);
			float w;
			mat_trans_nodiv_nomod(sphere.center.x, sphere.center.y, sphere.center.z, sphere.center.x, sphere.center.y, sphere.center.z, w);
			(void)w;
			sphere.radius *= go->maxWorldScale;

            if (!global_needsNoClip) {
                auto local_frustumTestResult = cam->frustumTestSphereNear(&sphere);
                if ( local_frustumTestResult == camera_t::SPHEREOUTSIDE) {
                    // printf("Outside local frustum cull\n");
                    continue;
                }

                if (local_frustumTestResult == camera_t::SPHEREBOUNDARY_NEAR) {
                    clippingRequired = 1 + textured;
                }
            }

            //isTextured, isNormaled, isColored, small_xyz, pad_xyz, small_uv
            unsigned selector = meshlet->flags;

            // template<bool hasTexture, bool small_xyz, bool forClip>
            unsigned smallSelector = ((selector & 8) ? 1 : 0) | clippingRequired * 2;

            dce_set_mat_decode(
                meshlet->boundingSphere.radius / 32767.0f,
                meshlet->boundingSphere.center.x,
                meshlet->boundingSphere.center.y,
                meshlet->boundingSphere.center.z
            );

            {

                mat_load(&cam->devViewProjScreen);
                mat_apply((matrix_t*)&go->ltw);

                if (selector & 8) {
                    // mat_load(&mtx);
                    mat_apply(&DCE_MESHLET_MAT_DECODE);
                } else {
                    // mat_load(&mtx);
                }
                tnlMeshletTransformSelector[smallSelector](OCR_SPACE, &go->mesh->data[meshlet->vertexOffset], meshlet->vertexCount, meshlet->vertexSize);
            }

            if (textured) {
                unsigned uvOffset = (selector & 8) ? (3 * 2) : (3 * 4);
                if (selector & 16) {
                    uvOffset += 1 * 2;
                }

                unsigned small_uv = (selector & 32) ? 1 : 0;
                tnlMeshletCopyUVsSelector[small_uv](OCR_SPACE, &go->mesh->data[meshlet->vertexOffset] + uvOffset, meshlet->vertexCount, meshlet->vertexSize);
            }

            if (selector & 4) {
                unsigned colOffset = (selector & 8) ? (3 * 2) : (3 * 4);
                if (selector & 16) {
                    colOffset += 1 * 2;
                }

                colOffset += (selector & 32) ? 2 : 4;

                colOffset += (selector & 2) ? 4 : 0;

                unsigned dstColOffset = textured ? offsetof(pvr_vertex64_t, a) : offsetof(pvr_vertex32_ut, a);
                dce_set_mat_vertex_color(&residual, &material);
                mat_load(&DCE_MESHLET_MAT_VERTEX_COLOR);
                tnlMeshletVertexColorSelector[0](OCR_SPACE + dstColOffset, (int8_t*)&go->mesh->data[meshlet->vertexOffset] + colOffset, meshlet->vertexCount, meshlet->vertexSize);
            } else if (go->bakedColors && !hasPointLights) {
				// colorsBase
				unsigned dstColOffset = textured ? offsetof(pvr_vertex64_t, a) : offsetof(pvr_vertex32_ut, a);
                dce_set_mat_vertex_color(&residual, &material);
                mat_load(&DCE_MESHLET_MAT_VERTEX_COLOR);
                tnlMeshletVertexColorBakedSelector[0](OCR_SPACE + dstColOffset, &go->bakedColors[submesh_num][colorsBase], meshlet->vertexCount);
			} else {
                unsigned dstColOffset = textured ? offsetof(pvr_vertex64_t, a) : offsetof(pvr_vertex32_ut, a);
                tnlMeshletFillResidualSelector[0](OCR_SPACE + dstColOffset, meshlet->vertexCount, &residual);
            }

            if (cntDiffuse) {
                unsigned normalOffset = (selector & 8) ? (3 * 2) : (3 * 4);
                if (selector & 16) {
                    normalOffset += 1 * 2;
                }

                normalOffset += (selector & 32) ? 2 : 4;

                unsigned dstColOffset = textured ? offsetof(pvr_vertex64_t, a) : offsetof(pvr_vertex32_ut, a);

                unsigned pass1 = cntDiffuse > 4? 4 : cntDiffuse;
                unsigned pass2 = cntDiffuse > 4 ? cntDiffuse - 4 : 0;

                
                unsigned normalSelector = (pass1 - 1);
                mat_load((matrix_t*)&uniformObject.dir[0][0][0]);
                auto normalPointer = &go->mesh->data[meshlet->vertexOffset] + normalOffset;
                auto vtxSize = meshlet->vertexSize;
                tnlMeshletDiffuseColorSelector[normalSelector](OCR_SPACE + dstColOffset, normalPointer, meshlet->vertexCount, vtxSize, &lightDiffuseColors[0]);
            
                if (pass2) {
                    unsigned normalSelector = (pass2 - 1);
                    mat_load((matrix_t*)&uniformObject.dir[1][0][0]);
                    tnlMeshletDiffuseColorSelector[normalSelector](OCR_SPACE + dstColOffset, normalPointer, meshlet->vertexCount, vtxSize, &lightDiffuseColors[4]);
                }
            }

			if (hasPointLights) {
				unsigned normalOffset = (selector & 8) ? (3 * 2) : (3 * 4);
                if (selector & 16) {
                    normalOffset += 1 * 2;
                }

                normalOffset += (selector & 32) ? 2 : 4;

				unsigned dstColOffset = textured ? offsetof(pvr_vertex64_t, a) : offsetof(pvr_vertex32_ut, a);
				auto normalPointer = &go->mesh->data[meshlet->vertexOffset] + normalOffset;
                auto vtxSize = meshlet->vertexSize;

				unsigned smallSelector = ((selector & 8) ? 1 : 0);

				mat_load((matrix_t*)&go->ltw);
                if (selector & 8) {
                    // mat_load(&mtx);
                    mat_apply(&DCE_MESHLET_MAT_DECODE);
                } else {
                    // mat_load(&mtx);
                }

				matrix_t mtx;
				mat_store(&mtx);

				for (auto point = point_lights; *point; point++) {
					if ((*point)->gameObject->isActive()) {
						float dist = length(sub((*point)->gameObject->ltw.pos, sphere.center));
						float maxDist = sphere.radius + (*point)->Range;
						if (dist < maxDist) {
							tnlMeshletPointColorSelector[smallSelector](&go->mesh->data[meshlet->vertexOffset], OCR_SPACE + dstColOffset, normalPointer, meshlet->vertexCount, vtxSize, *point, det, &mtx, (matrix_t*)&go->ltw, go->materials[submesh_num]);
						}
					}
				}
			}

            auto indexData = (int8_t*)&go->mesh->data[meshlet->indexOffset];

            if (!clippingRequired) {
                submitMeshletSelector[textured](OCR_SPACE, indexData, meshlet->indexCount);
            } else {
                clipAndsubmitMeshletSelector[textured](OCR_SPACE, indexData, meshlet->indexCount);
            }
        }
    }
}

// Helper function: returns true if the point (x, y) is inside the convex quad
// defined by vertices v0, v1, v2, v3 (assumed ordered).
bool IsPointInsideQuad(float x, float y,
					   const V3d *v0, const V3d *v1,
					   const V3d *v2, const V3d *v3)
{
	// Compute cross product for each edge.
	// For edge from v0 to v1
	float cross0 = (v1->x - v0->x) * (y - v0->y) - (v1->y - v0->y) * (x - v0->x);
	// For edge from v1 to v2
	float cross1 = (v2->x - v1->x) * (y - v1->y) - (v2->y - v1->y) * (x - v1->x);
	// For edge from v2 to v3
	float cross2 = (v3->x - v2->x) * (y - v2->y) - (v3->y - v2->y) * (x - v2->x);
	// For edge from v3 to v0
	float cross3 = (v0->x - v3->x) * (y - v3->y) - (v0->y - v3->y) * (x - v3->x);

	// If the point is inside the quad, then all cross products must have the same sign.
	if ((cross0 >= 0.0f && cross1 >= 0.0f && cross2 >= 0.0f && cross3 >= 0.0f) ||
		(cross0 <= 0.0f && cross1 <= 0.0f && cross2 <= 0.0f && cross3 <= 0.0f))
	{
		return true;
	}
	return false;
}

// Helper function: returns true if the axis-aligned quad defined by
// snappedLeft, snappedTop, snappedRight, snappedBottom is fully contained
// within the convex quad defined by vertices v0, v1, v2, v3.
bool ValidateSnappedQuadContained(float snappedLeft, float snappedTop,
								  float snappedRight, float snappedBottom,
								  const V3d *v0, const V3d *v1,
								  const V3d *v2, const V3d *v3)
{
	// Define the four corners of the snapped quad.
	// Assuming screen-space coordinates where (snappedLeft, snappedTop) is the top-left, etc.
	float x0 = snappedLeft, y0 = snappedTop;	 // top-left
	float x1 = snappedRight, y1 = snappedTop;	 // top-right
	float x2 = snappedLeft, y2 = snappedBottom;	 // bottom-left
	float x3 = snappedRight, y3 = snappedBottom; // bottom-right

	// Validate each corner.
	bool contained = true;
	contained = contained && IsPointInsideQuad(x0, y0, v0, v1, v2, v3);
	contained = contained && IsPointInsideQuad(x1, y1, v0, v1, v2, v3);
	contained = contained && IsPointInsideQuad(x2, y2, v0, v1, v2, v3);
	contained = contained && IsPointInsideQuad(x3, y3, v0, v1, v2, v3);

	assert(contained);
	return contained;
}

bool IsQuadConvex(const V3d* v0, const V3d* v1, const V3d* v2, const V3d* v3)
{
    // Utility lambda to compute the 2D cross product of two vectors (ignoring z).
    auto Cross2D = [](float ax, float ay, float bx, float by) -> float {
        return ax * by - ay * bx;
    };

    // Compute the vectors for each edge.
    float dx0 = v1->x - v0->x;
    float dy0 = v1->y - v0->y;
    
    float dx1 = v2->x - v1->x;
    float dy1 = v2->y - v1->y;
    
    float dx2 = v3->x - v2->x;
    float dy2 = v3->y - v2->y;
    
    float dx3 = v0->x - v3->x;
    float dy3 = v0->y - v3->y;
    
    // Compute cross products for each consecutive edge pair.
    float cross0 = Cross2D(dx0, dy0, dx1, dy1);
    float cross1 = Cross2D(dx1, dy1, dx2, dy2);
    float cross2 = Cross2D(dx2, dy2, dx3, dy3);
    float cross3 = Cross2D(dx3, dy3, dx0, dy0);
    
    // The quad is convex if all cross products are either non-negative or non-positive.
    bool allNonNegative = (cross0 >= 0.0f && cross1 >= 0.0f && cross2 >= 0.0f && cross3 >= 0.0f);
    bool allNonPositive = (cross0 <= 0.0f && cross1 <= 0.0f && cross2 <= 0.0f && cross3 <= 0.0f);
    
    return (allNonNegative || allNonPositive);
}

void renderQuads(camera_t* cam, game_object_t* go) {
	{
		mat_load(&cam->devViewProjScreen);
		mat_apply((matrix_t*)&go->ltw);

		// std::cout << "mesh: " << go->mesh << std::endl;
		
		int16_t quadOffset = *(int16_t*)go->mesh->quadData;
		int8_t quadCount = go->mesh->quadData[2];
		int8_t vtxCount = go->mesh->quadData[3];
		
		// std::cout << "quads: " << quadOffset << " num " << quadCount << " verts " << (int)vtxCount << std::endl;

		V3d* srcVtx = (V3d*)(quadOffset + quadCount * 8 + go->mesh->quadData);
		V3d* dstVtx = (V3d*)OCR_SPACE;
		// float *zBuffer = (float*)(OCR_SPACE + (32 * 12));
		
		do {
			float x, y, z, w;
			mat_trans_nodiv_nomod(srcVtx->x, srcVtx->y, srcVtx->z, x, y, z, w);
			(void)z;

			w = 1/w;

			x*= w;
			y*= w;

			dstVtx->x = x;
			dstVtx->y = y;
			dstVtx->z = w;

			srcVtx++;
			dstVtx++;
		} while(--vtxCount);
		
		int16_t* quadIndex = (int16_t*)(go->mesh->quadData + quadOffset);
		uint8_t* transVtx = OCR_SPACE;
		int16_t cIndex;

		for (size_t quadIdx = 0; quadIdx < quadCount*4; quadIdx+=4) {
			// submit quad
			
			const V3d* cVtx0 = (V3d*)(transVtx+quadIndex[quadIdx + 0]);
			const V3d* cVtx1 = (V3d*)(transVtx+quadIndex[quadIdx + 1]);
			const V3d* cVtx2 = (V3d*)(transVtx+quadIndex[quadIdx + 2]);
			const V3d* cVtx3 = (V3d*)(transVtx+quadIndex[quadIdx + 3]);

			if (cVtx0->z <= 0) continue;
			if (cVtx1->z <= 0) continue;
			if (cVtx2->z <= 0) continue;
			if (cVtx3->z <= 0) continue;

			float minX = cVtx0->x;
			if(cVtx1->x < minX) minX = cVtx1->x;
			if(cVtx2->x < minX) minX = cVtx2->x;
			if(cVtx3->x < minX) minX = cVtx3->x;
			if (minX < 0) minX = 0;

			float maxX = cVtx0->x;
			if(cVtx1->x > maxX) maxX = cVtx1->x;
			if(cVtx2->x > maxX) maxX = cVtx2->x;
			if(cVtx3->x > maxX) maxX = cVtx3->x;
			if (maxX > 639) maxX = 639;

			float minY = cVtx0->y;
			if(cVtx1->y < minY) minY = cVtx1->y;
			if(cVtx2->y < minY) minY = cVtx2->y;
			if(cVtx3->y < minY) minY = cVtx3->y;
			if (minY < 0) minY = 0;

			float maxY = cVtx0->y;
			if(cVtx1->y > maxY) maxY = cVtx1->y;
			if(cVtx2->y > maxY) maxY = cVtx2->y;
			if(cVtx3->y > maxY) maxY = cVtx3->y;
			if (maxY > 479) maxY = 479;

			if (minX >= maxX || minY>=maxY) {
				continue;
			}

			float sum1 = cVtx0->x * cVtx1->y +
						  cVtx1->x * cVtx2->y +
						  cVtx2->x * cVtx3->y +
						  cVtx3->x * cVtx0->y;

			float sum2 = cVtx0->y * cVtx1->x +
						  cVtx1->y * cVtx2->x +
						  cVtx2->y * cVtx3->x +
						  cVtx3->y * cVtx0->x;

			float areax2 = sum1 - sum2;
			if (fabsf(areax2) < 8192) continue;

			// std::cout << "Quad Area " << fabsf(areax2 * 0.5f) << std::endl;

			const float Y1 = cVtx0->y;
			const float Y2 = cVtx1->y;
			const float Y3 = cVtx2->y;
			const float Y4 = cVtx3->y;
		
			const float X1 = cVtx0->x;
			const float X2 = cVtx1->x;
			const float X3 = cVtx2->x;
			const float X4 = cVtx3->x;

			float DX12 = (X1 - X2);
			float DX23 = (X2 - X3);
			float DX31 = (X3 - X4);
			float DX41 = (X4 - X1);
		
			float DY12 = (Y1 - Y2);
			float DY23 = (Y2 - Y3);
			float DY31 = (Y3 - Y4);
			float DY41 = (Y4 - Y1);

			if (areax2 > 0) {
				DX12 = (X2 - X1);
				DX23 = (X3 - X2);
				DX31 = (X4 - X3);
				DX41 = (X1 - X4);
			
				DY12 = (Y2 - Y1);
				DY23 = (Y3 - Y2);
				DY31 = (Y4 - Y3);
				DY41 = (Y1 - Y4);
			}

			float C1 = DY12 * X1 - DX12 * Y1;
			float C2 = DY23 * X2 - DX23 * Y2;
			float C3 = DY31 * X3 - DX31 * Y3;
			float C4 = DY41 * X4 - DX41 * Y4;

			float det = cVtx0->x * (cVtx1->y - cVtx2->y) +
						cVtx1->x * (cVtx2->y - cVtx0->y) +
						cVtx2->x * (cVtx0->y - cVtx1->y);
			
			float absDet = (det < 0.0f) ? -det : det;
			if (absDet < 0.001f) {
				// std::cout << "Cannot compute a valid plane from quad vertices.\n";
				continue;
			}
			float A = (cVtx0->z * (cVtx1->y - cVtx2->y) +
					cVtx1->z * (cVtx2->y - cVtx0->y) +
					cVtx2->z * (cVtx0->y - cVtx1->y)) / det;
			float B = (cVtx0->z * (cVtx2->x - cVtx1->x) +
					cVtx1->z * (cVtx0->x - cVtx2->x) +
					cVtx2->z * (cVtx1->x - cVtx0->x)) / det;
			float C = (cVtx0->z * (cVtx1->x * cVtx2->y - cVtx2->x * cVtx1->y) +
					cVtx1->z * (cVtx2->x * cVtx0->y - cVtx0->x * cVtx2->y) +
					cVtx2->z * (cVtx0->x * cVtx1->y - cVtx1->x * cVtx0->y)) / det;

			unsigned iMinX = minX/20.f;
			unsigned iMaxX = (maxX + 19.99f)/20.f;
			if (iMaxX == 32) { iMaxX--; }

			unsigned iMinY = minY/16.f;
			unsigned iMaxY = (maxY + 15.99f)/16.f;
			if (iMaxY == 32) { iMaxY--; }

			#if defined(DC_SIM)
			assert(iMinX>=0 && iMaxX<=32);
			assert(iMinY>=0 && iMaxY<=32);
			#endif
			static matrix_t inQuadMtx;

			inQuadMtx[0][0] = -DY12;
			inQuadMtx[0][1] = -DY23;
			inQuadMtx[0][2] = -DY31;
			inQuadMtx[0][3] = -DY41;

			inQuadMtx[1][0] = DX12;
			inQuadMtx[1][1] = DX23;
			inQuadMtx[1][2] = DX31;
			inQuadMtx[1][3] = DX41;

			inQuadMtx[2][0] = C1;
			inQuadMtx[2][1] = C2;
			inQuadMtx[2][2] = C3;
			inQuadMtx[2][3] = C4;

			mat_load(&inQuadMtx);

			float fy = iMinY * 16;
			for (unsigned y = iMinY; y <= iMaxY; y++, fy+=16)
			{
				float fx = iMinX * 20;
				for (unsigned x = iMinX; x <= iMaxX; x++, fx+=20)
				{
					float x_ps = fx;
					float y_ps = fy;

					// float Xhs12 = C1 + DX12 * y_ps - DY12 * x_ps;
					// float Xhs23 = C2 + DX23 * y_ps - DY23 * x_ps;
					// float Xhs31 = C3 + DX31 * y_ps - DY31 * x_ps;
					// float Xhs41 = C4 + DX41 * y_ps - DY41 * x_ps;

					float Xhs12;
					float Xhs23;
					float Xhs31;
					float Xhs41;
					
					mat_trans_nodiv_nomod(x_ps, y_ps, 1, Xhs12, Xhs23, Xhs31, Xhs41);

					bool inQuad = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;

					if (!inQuad) {
						continue;
					}

					float zTL = A * x_ps  + B * y_ps + C;

					x_ps = fx + 20;
					y_ps = fy;
					
					mat_trans_nodiv_nomod(x_ps, y_ps, 1, Xhs12, Xhs23, Xhs31, Xhs41);

					inQuad = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;
					if (!inQuad) {
						continue;
					}

					float zTR = A * x_ps  + B * y_ps + C;

					x_ps = fx;
					y_ps = fy + 16;
					
					mat_trans_nodiv_nomod(x_ps, y_ps, 1, Xhs12, Xhs23, Xhs31, Xhs41);

					inQuad = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;
					if (!inQuad) {
						continue;
					}
					float zBL = A * x_ps  + B * y_ps + C;

					x_ps = fx + 20;
					y_ps = fy + 16;
					
					mat_trans_nodiv_nomod(x_ps, y_ps, 1, Xhs12, Xhs23, Xhs31, Xhs41);

					inQuad = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;
					if (!inQuad) {
						continue;
					}
					float zBR = A * x_ps  + B * y_ps + C;


					float minZ = zTL;
					if(zTR < minZ)
						minZ = zTR;
					if(zBL < minZ)
						minZ = zBL;
					if(zBR < minZ)
						minZ = zBR;

					if (minZ < 0) {
						continue;
					}

					#if defined(DC_SIM)
					ValidateSnappedQuadContained(fx, fy, fx+19, fy+15, cVtx0, cVtx1, cVtx2, cVtx3);
					#endif
					if (zBuffer[y][x] < minZ) {
						zBuffer[y][x] = minZ;
					}
				}
			}
		}
	}
}

// assert(IsQuadConvex(cVtx0, cVtx1, cVtx2, cVtx3));

// /// --- Compute bounding box manually ---
// float minX = cVtx0->x;
// if(cVtx1->x < minX) minX = cVtx1->x;
// if(cVtx2->x < minX) minX = cVtx2->x;
// if(cVtx3->x < minX) minX = cVtx3->x;
// if (minX < 0) minX = 0;

// float maxX = cVtx0->x;
// if(cVtx1->x > maxX) maxX = cVtx1->x;
// if(cVtx2->x > maxX) maxX = cVtx2->x;
// if(cVtx3->x > maxX) maxX = cVtx3->x;
// if (maxX > 640) maxX = 640;

// float minY = cVtx0->y;
// if(cVtx1->y < minY) minY = cVtx1->y;
// if(cVtx2->y < minY) minY = cVtx2->y;
// if(cVtx3->y < minY) minY = cVtx3->y;
// if (minY < 0) minY = 0;

// float maxY = cVtx0->y;
// if(cVtx1->y > maxY) maxY = cVtx1->y;
// if(cVtx2->y > maxY) maxY = cVtx2->y;
// if(cVtx3->y > maxY) maxY = cVtx3->y;
// if (maxY > 480) maxY = 480;

// if (minX >= maxX | minY>=maxY) {
// 	continue;
// }
// std::cout << "Quad: " << areax2 << std::endl;
// // At this point, (insLeft, insTop) and (insRight, insBottom)
// // define an axis–aligned rectangle guaranteed to be contained within the quad.

// // 2. Clip the bounding box to the screen: 0 <= x <= 640 and 0 <= y <= 480.
// if(minX < 0.0f)
// 	minX = 0.0f;
// if(minY < 0.0f)
// 	minY = 0.0f;
// if(maxX > 640.0f)
// 	maxX = 640.0f;
// if(maxY > 480.0f)
// 	maxY = 480.0f;

// // 3. Snap the clipped bounding box to a 32x16 grid.
// // Tile dimensions:
// constexpr uint32_t tileWidth  = 32;
// constexpr uint32_t tileHeight = 16;

// // For left and top we want the smallest grid multiple that is >= the coordinate.
// uint32_t iLeft = (uint32_t)(minX / tileWidth);
// if ((float)iLeft * tileWidth < minX)
// 	iLeft++; // round up if needed.
// uint32_t snappedLeft = iLeft * tileWidth;

// uint32_t iTop = (uint32_t)(minY / tileHeight);
// if ((float)iTop * tileHeight < minY)
// 	iTop++;
// uint32_t snappedTop = iTop * tileHeight;

// // For right and bottom we need a full tile to fit in the bounding box.
// // So we require that a cell starting at L (or T) fits entirely, meaning L + tileWidth <= maxX.
// if(maxX < tileWidth || maxY < tileHeight) {
// 	// std::cout << "Quad rejected: insufficient tile coverage.\n";
// 	// return;
// 	continue;
// }

// // Compute the maximum grid cell starting position that still allows a full tile.
// // Since values are nonnegative, simple integer division works as floor.
// uint32_t iRight = (uint32_t)((maxX - tileWidth) / tileWidth);
// uint32_t snappedRight = iRight * tileWidth + tileWidth;

// uint32_t iBottom = (uint32_t)((maxY - tileHeight) / tileHeight);
// uint32_t snappedBottom = iBottom * tileHeight + tileHeight;

// // Validate that the snapped quad covers at least one full tile.
// if (snappedLeft >= snappedRight || snappedTop >= snappedBottom) {
// 	// std::cout << "Quad rejected: insufficient tile coverage.\n";
// 	// return;
// 	continue;
// }

// // 4. Compute the plane for depth interpolation: z(x, y) = A*x + B*y + C.
// // Using vertices cVtx0, cVtx1, and cVtx2.
// float det = cVtx0->x * (cVtx1->y - cVtx2->y) +
// 			cVtx1->x * (cVtx2->y - cVtx0->y) +
// 			cVtx2->x * (cVtx0->y - cVtx1->y);
// // Inline absolute value.
// float absDet = (det < 0.0f) ? -det : det;
// if (absDet < 0.000001f) {
// 	// std::cout << "Cannot compute a valid plane from quad vertices.\n";
// 	continue;
// }
// float A = (cVtx0->z * (cVtx1->y - cVtx2->y) +
// 		cVtx1->z * (cVtx2->y - cVtx0->y) +
// 		cVtx2->z * (cVtx0->y - cVtx1->y)) / det;
// float B = (cVtx0->z * (cVtx2->x - cVtx1->x) +
// 		cVtx1->z * (cVtx0->x - cVtx2->x) +
// 		cVtx2->z * (cVtx1->x - cVtx0->x)) / det;
// float C = (cVtx0->z * (cVtx1->x * cVtx2->y - cVtx2->x * cVtx1->y) +
// 		cVtx1->z * (cVtx2->x * cVtx0->y - cVtx0->x * cVtx2->y) +
// 		cVtx2->z * (cVtx0->x * cVtx1->y - cVtx1->x * cVtx0->y)) / det;

// // 5. Evaluate depth (z) at the snapped quad corners.
// float zTL = A * snappedLeft  + B * snappedTop    + C;
// float zTR = A * snappedRight + B * snappedTop    + C;
// float zBL = A * snappedLeft  + B * snappedBottom + C;
// float zBR = A * snappedRight + B * snappedBottom + C;

// // 6. Find the minimum depth value.
// float minZ = zTL;
// if(zTR < minZ)
// 	minZ = zTR;
// if(zBL < minZ)
// 	minZ = zBL;
// if(zBR < minZ)
// 	minZ = zBR;

// if (minZ < 0) {
// 	continue;
// }

// std::cout << "Quad: " << snappedLeft << ", " << snappedTop << " ~ " << snappedRight << ", " << snappedBottom << " minZ: " << minZ << std::endl;

// assert(minX <= snappedLeft);
// assert(minY <= snappedTop);
// assert(maxX >= snappedRight);
// assert(maxY >= snappedBottom);

// assert(ValidateSnappedQuadContained(snappedLeft, snappedTop, snappedRight, snappedBottom, cVtx0, cVtx1, cVtx2, cVtx3));

// // std::cout << "Minimum depth (z) value in the snapped quad: " << minZ << "\n";
// uint8_t* rgb = (uint8_t*)&go->mesh;

// vtx[0].flags = PVR_CMD_VERTEX;
// vtx[0].x = snappedLeft;
// vtx[0].y = snappedTop;
// vtx[0].z = minZ;
// vtx[0].a = 0.2f;
// vtx[0].r = minZ * 10;
// vtx[0].g = minZ * 10;
// vtx[0].b = minZ * 10;

// vtx[1].flags = PVR_CMD_VERTEX;
// vtx[1].x = snappedRight;
// vtx[1].y = snappedTop;
// vtx[1].z = minZ;
// vtx[1].a = 0.2f;
// vtx[1].r = minZ * 10;
// vtx[1].g = minZ * 10;
// vtx[1].b = minZ * 10;

// vtx[2].flags = PVR_CMD_VERTEX;
// vtx[2].x = snappedLeft;
// vtx[2].y = snappedBottom;
// vtx[2].z = minZ;
// vtx[2].a = 0.2f;
// vtx[2].r = minZ * 10;
// vtx[2].g = minZ * 10;
// vtx[2].b = minZ * 10;

// vtx[3].flags = PVR_CMD_VERTEX_EOL; // End of triangle strip
// vtx[3].x = snappedRight;
// vtx[3].y = snappedBottom;
// vtx[3].z = minZ;
// vtx[3].a = 0.2f;
// vtx[3].r = minZ * 10;
// vtx[3].g = minZ * 10;
// vtx[3].b = minZ * 10;


// pvr_prim(vtx, sizeof(pvr_vertex32_ut));
// pvr_prim(vtx + 1, sizeof(pvr_vertex32_ut));
// pvr_prim(vtx + 2, sizeof(pvr_vertex32_ut));
// pvr_prim(vtx + 3, sizeof(pvr_vertex32_ut));	

// extern "C" cus also used by KOS
extern "C" const char* getExecutableTag() {
	return "tlj "  ":" ;
}
const char* choices_prompt;
const char** choices_options;
int choice_chosen = -1;
int choice_current = 0;

const char* lookAtMessage = nullptr;
const char* messageSpeaker = nullptr;
const char* messageText = nullptr;

void setGameObject(component_type_t type, component_base_t* component, native::game_object_t* gameObject) {
    if (type >= ct_interaction) {
        auto interaction = (interaction_t*)component;
        interaction->gameObject = gameObject;
    } else {
        component->gameObject = gameObject;
    }
}

void animator_t::update(float deltaTime) {
    
    for (size_t i = 0; i < num_bound_animations; ++i) {
        auto& boundAnim = bound_animations[i];
		auto& currentTime = boundAnim.currentTime;
		currentTime += deltaTime;
        for (size_t j = 0; j < boundAnim.animation->num_tracks; ++j) {
            auto& track = boundAnim.animation->tracks[j];
            auto& currentFrame = boundAnim.currentFrames[j];
            while (currentFrame < track.num_keys - 1 && currentTime >= track.times[currentFrame + 1]) {
                ++currentFrame;
            }
			bool fakeInterpolation = false;
			float effectiveTime = currentTime;
            if (currentFrame >= track.num_keys - 1) {
				currentFrame = track.num_keys - 2;
				effectiveTime = track.times[currentFrame + 1];
				fakeInterpolation = true;
            }
            float t = (effectiveTime - track.times[currentFrame]) / (track.times[currentFrame + 1] - track.times[currentFrame]);
            auto value = track.values[currentFrame];
            auto nextValue = track.values[currentFrame + 1];

			if (fakeInterpolation) {
				value = nextValue;
			}

            if (boundAnim.bindings[j] == SIZE_MAX) {
                continue;
            }
            auto& target = gameObjects[boundAnim.bindings[j]];
            switch (track.property_key) {
                case Transform_m_LocalPosition_x:
                    target->position.x = value + t * (nextValue - value);
                    break;
                case Transform_m_LocalPosition_y:
                    target->position.y = value + t * (nextValue - value);
                    break;
                case Transform_m_LocalPosition_z:
                    target->position.z = value + t * (nextValue - value);
                    break;
                case Transform_localEulerAnglesRaw_x:
                    target->rotation.x = value + t * (nextValue - value);
                    break;
                case Transform_localEulerAnglesRaw_y:
                    target->rotation.y = value + t * (nextValue - value);
                    break;
                case Transform_localEulerAnglesRaw_z:
                    target->rotation.z = value + t * (nextValue - value);
                    break;
                case Transform_m_LocalScale_x:
                    target->scale.x = value + t * (nextValue - value);
                    break;
                case Transform_m_LocalScale_y:
                    target->scale.y = value + t * (nextValue - value);
                    break;
                case Transform_m_LocalScale_z:
                    target->scale.z = value + t * (nextValue - value);
                    break;
				case GameObject_IsActive:
					target->setActive(value == 1);
					break;
				case AudioSource_Volume:
					{
						auto audioSource = target->getComponent<audio_source_t>();
						if (audioSource) {
							audioSource->volume = value + t * (nextValue - value);
						}
					}
					break;
				case AudioSource_Enabled:
					{
						auto audioSource = target->getComponent<audio_source_t>();
						if (audioSource) {
							audioSource->setEnabled(value == 1);
						}
					}
					break;

				case Camera_FOV:
					{
						auto camera = target->getComponent<camera_t>();
						if (camera) {
							camera->fov = value + t * (nextValue - value);
						}
					}
					break;

				case MeshRenderer_Enabled:
				{
					target->mesh_enabled = value == 1;
				}
				break;
				case MeshRenderer_material_Color_a:
				{
					if (target->mesh) {
						target->materials[0]->color.alpha =  value + t * (nextValue - value);
					}
				}
				break;
				case MeshRenderer_material_Color_b:
				{
					if (target->mesh) {
						target->materials[0]->color.blue =  value + t * (nextValue - value);
					}
				}
				break;
				case MeshRenderer_material_Color_g:
				{
					if (target->mesh) {
						target->materials[0]->color.green =  value + t * (nextValue - value);
					}
				}
				break;
				case MeshRenderer_material_Color_r:
				{
					if (target->mesh) {
						target->materials[0]->color.red =  value + t * (nextValue - value);
					}
				}
				break;

				case MeshRenderer_m_Materials_0:
				{
					if (target->mesh) {
						target->materials[0] = materials[value];
					}
				}
            }
        }
    }

	for (size_t i = 0; i < num_bound_animations; ++i) {
		auto& boundAnim = bound_animations[i];
		if (boundAnim.currentTime >= boundAnim.animation->maxTime) {
			boundAnim.currentTime = 0;
			
			for (size_t j = 0; j < boundAnim.animation->num_tracks; ++j) {
				boundAnim.currentFrames[j] = 0;
			}
		}
	}
}

bool native::game_object_t::isActive() const {
	if (inactiveFlags & goi_inactive) {
		return false;
	} else if (parent) {
		return parent->isActive();
	}
	return true;
}

void native::game_object_t::computeActiveState() {
	// if (inactiveFlags & goi_inactive) {

	// }
}

void recusrive_awake(game_object_t* go) {
	audio_source_t** audioSources = go->getComponents<audio_source_t>();
	if (audioSources) {
		do {
			auto audioSource = *audioSources;
			if (audioSource->enabled) {
				audioSource->awake();
			}
		} while(*++audioSources);
	}

	auto childNum = go->children;
	while(*childNum != SIZE_MAX) {
		auto child = gameObjects[*childNum++];
		if (child->isActive()) {
			recusrive_awake(child);
		}
	}
}

void recusrive_disable(game_object_t* go) {
	audio_source_t** audioSources = go->getComponents<audio_source_t>();
	if (audioSources) {
		do {
			auto audioSource = *audioSources;
			audioSource->disable();
		} while(*++audioSources);
	}

	auto childNum = go->children;
	while(*childNum != SIZE_MAX) {
		auto child = gameObjects[*childNum++];
		recusrive_disable(child);
	}
}

void native::game_object_t::setActive(bool active) {
	bool wasActive = isActive();
	inactiveFlags = !active ? goi_inactive : 0;

	bool nowActive = isActive();

	if (!wasActive && nowActive) {
		recusrive_awake(this);
	}

	if (wasActive && !nowActive) {
		recusrive_disable(this);
	}
	// if (active && (inactiveFlags & goi_inactive)) {
	// 	inactiveFlags &= ~goi_inactive;
	// 	if (!inactiveFlags) {
	// 		// enable children
	// 		std::cout << "enable children" << std::endl;
	// 	}
	// }else if (!active && !(inactive & goi_inactive)) {
	// 	inactive |= goi_inactive;
	// 	if (inactive) {
	// 		// disable children
	// 		std::cout << "disable children" << std::endl;
	// 	}
	// }
}

void proximity_interactable_t::update(float deltaTime) {
	auto v = gameObjects[this->playaIndex]->ltw.pos;
	auto y = gameObject->ltw.pos;
	auto distance = sqrt(
		(v.x - y.x) * (v.x - y.x) +
		(v.y - y.y) * (v.y - y.y) +
		(v.z - y.z) * (v.z - y.z)
	);

	if (distance < radius && !hasTriggered) {
		hasTriggered = true;
		// fire all triggers
		auto component = gameObject->getComponents<interaction_t>();

		if (component) {
			do {
				(*component)->interact();
			} while(*++component);
		}
	} else if (distance > radius && hasTriggered) {
		if (multipleShot) {
			hasTriggered = false;
		}
	}
}

bool interactable_t::showMessage() {
	if (messages) {
		if (!messages[inspectionCounter]) {
			inspectionCounter = 0;
			// TODO
			//messaging.Unfocused(this);
			messageSpeaker = nullptr;
			messageText = nullptr;
			return false;
		} else {
			const char* message = messages[inspectionCounter++];
			while (messages[inspectionCounter] && strlen(messages[inspectionCounter]) == 0) {
				inspectionCounter++;
			}

			bool isLast = messages[inspectionCounter] == nullptr;

			assert(!strncmp("##", message, 2) == 0);

			// TODO
			// messaging.TypeMessage(this, message, (!isLast || AlwaysShowHasModeIndicator) && ShowHasMoreIndicator, SpeakerName);
			messageSpeaker = speakerName;
			messageText = message;
			return true;
		}
	}
	return false;
}
pavo_interaction_delegate_t interactable_t::onInteraction()
{
	// TODO messaging
	if (/*messaging.NonPavoIsTyping || */showMessage())
	{
		return [this]() { return this->onInteraction(); };
	}
	else
	{
		pavo_state_t::popEnv(&oldState);
		return nullptr;
	}
}

void interactable_t::focused() {
	if (showMessage()) {
		pavo_state_t::pushEnv({.canMove = false, .canRotate = false, .onInteraction = [this]() { return this->onInteraction(); }}, &oldState);
	}
}
void interactable_t::interact() {
	// TODO: this is very partial
	auto interactions = gameObject->getComponents<interaction_t>();
	if (interactions) {
		do {
			(*interactions)->interact();
			assert(!(*interactions)->blocking);
		} while(*++interactions);
	}
}

// interactions
void game_object_activeinactive_t::interact() {
	if (gameObjectToToggle != SIZE_MAX) {
		gameObjects[gameObjectToToggle]->setActive(setTo);
	}
}

void timed_activeinactive_t::update(float deltaTime) {
	if (!triggered) {
		return;
	}
	if (countDown > 0) {
		countDown -= deltaTime;
		if (countDown <= 0) {
			if (gameObjectToToggle != SIZE_MAX) {
				gameObjects[gameObjectToToggle]->setActive(setTo);
			}
			triggered = false;
		}
	}
}

void timed_activeinactive_t::interact() {
	triggered = true;
	countDown = delay;
}

void fadein_t::interact() {
	// TODO: actually fade in
	target->volume = targetVolume;
}

void show_message_t::interact() {
	if (messages) {
		nextMessage();
		if (timedHide) {
			timeToGo = time;
			if (timedHideCameraLock) {
				pavo_state_t::pushEnv({ .canMove = false, .canRotate = false, .onInteraction = [this]() { return this->onInteraction(); }}, &oldState);
			}
		} else {
			pavo_state_t::pushEnv({ .canMove = false, .canRotate = false, .onInteraction = [this]() { return this->onInteraction(); }}, &oldState);
		}
	}
}

pavo_interaction_delegate_t show_message_t::onInteraction() {
	if (/*messaging.NonPavoIsTyping ||*/ nextMessage())
	{
		return [this]() { return this->onInteraction(); };
	}
	else
	{
		pavo_state_t::popEnv(&oldState);
		return nullptr;
	}
}

bool show_message_t::nextMessage() {
	if (messages[currentMessageIndex] == nullptr || !oneShot) {
		if (messages[currentMessageIndex] == nullptr) {
			currentMessageIndex = 0;
		}
		
		messageSpeaker = nullptr;
		messageText = nullptr;
		// TODO: messaging unfocused (this)
		return false;
	} else {
		const char* msg = messages[currentMessageIndex++];
		while (messages[currentMessageIndex] && strlen(messages[currentMessageIndex]) == 0) {
			currentMessageIndex++;
		}

		bool isLast = messages[currentMessageIndex] == nullptr;

		// messaging.TypeMessage(this, msg, (!isLast || AlwaysShowHasMoreIndicator) && ShowHasMoreIndicator, SpeakerName);
		messageSpeaker = speakerName;
		messageText = msg;
		return true;
	}
}

void show_message_t::update(float deltaTime) {
	if (timedHide && timeToGo > 0) {
		timeToGo -= deltaTime;
		if (timeToGo <= 0) {
			if (nextMessage()) {
				timeToGo = time;
			} else {
				if (timedHideCameraLock) {
					pavo_state_t::popEnv(&oldState);
				}
			}
		}
	}
}

void teleporter_trigger_t::interact() {
	if (teleporterToTriggerIndex != SIZE_MAX) {
		game_object_t* teleporterGameObject = gameObjects[teleporterToTriggerIndex];
		if (auto teleporter = teleporterGameObject->getComponent<teleporter_t>()) {
			teleporter->tryTeleport();
			return;
		}
		// TODO: Also handle LevelLoader here
	}
}

Task zoom_in_out_t::doAnimation() {
	float startTime = 0;

	do {
		reactphysics3d::Transform(
			finalPosition  + (targetPosition - finalPosition) * (startTime / zoomInDuration),
			reactphysics3d::Quaternion::slerp(finalRotation, targetRotation, (startTime / zoomInDuration))
		).getOpenGLMatrix(&gameObjects[cameraIndex]->ltw.m00);
		startTime += timeDeltaTime;
		if (startTime > zoomInDuration) {
			startTime = zoomInDuration;
		}
		co_yield Step::Frame;
	} while (startTime < zoomInDuration);

	// Can't use wait here due to LTW ovewrites. TODO: FIX LTW OVEWRITES
	startTime = 0;
	do {
		reactphysics3d::Transform(
			targetPosition,
			targetRotation
		).getOpenGLMatrix(&gameObjects[cameraIndex]->ltw.m00);
		startTime += timeDeltaTime;
		co_yield Step::Frame;
	} while(startTime < inactiveDuration);

	startTime = 0;
	do {
		reactphysics3d::Transform(
			targetPosition  + (finalPosition - targetPosition) * (startTime / zoomOutDuration),
			reactphysics3d::Quaternion::slerp(targetRotation, finalRotation, (startTime / zoomOutDuration))
		).getOpenGLMatrix(&gameObjects[cameraIndex]->ltw.m00);
		startTime += timeDeltaTime;
		if (startTime > zoomOutDuration) {
			startTime = zoomOutDuration;
		}
		co_yield Step::Frame;
	} while (startTime < zoomOutDuration);
}
void zoom_in_out_t::interact() {
	reactphysics3d::Transform targetTransform;
	targetTransform.setFromOpenGL(&gameObjects[targetIndex]->ltw.m00);
	targetPosition = targetTransform.getPosition();
	targetRotation = targetTransform.getOrientation();

	reactphysics3d::Transform finalTransform;
	finalTransform.setFromOpenGL(&gameObjects[cameraIndex]->ltw.m00);
	finalPosition = finalTransform.getPosition();
	finalRotation = finalTransform.getOrientation();

	targetPosition = targetPosition  + (finalPosition - targetPosition) * startingDistance;
	targetRotation = reactphysics3d::Quaternion::slerp(targetRotation, finalRotation, startingDistance);

	queueCoroutine(this->doAnimation());
}

Task cant_move_t::delayDeactivate(float delay) {
	std::shared_ptr<pavo_flat_game_env_t> oldState;

	//State.PushState(new GameEnv(canMove: CanMove, canRotate: CanRotate, canLook: CanLook), ref oldState); 
	pavo_state_t::pushEnv({.canMove = canMove, .canRotate = canRotate, .canLook = canLook}, &oldState);
	co_yield WaitTime(delay);
	pavo_state_t::popEnv(&oldState);
}

void cant_move_t::interact() {
	queueCoroutine(this->delayDeactivate(delay));
}

void play_sound_t::interact() {
	source->volume = 1;
	source->play();
}

void object_dispenser_t::interact() {
	if (itemsAvailable > 0) {
		pavo_state_t::addItem(itemToDispense);
		itemsAvailable--;
		if (deactivateAfterDispensing)
		{
			gameObject->setActive(false);
		}
	}
}

void recusrive_game_object_activeinactive_t::interact() {
	if (this->gameObjectToToggle != SIZE_MAX) {
		auto gameObjectTT = gameObjects[this->gameObjectToToggle];
		gameObjectTT->mesh_enabled = false;
		// TODO: do this correctly
		gameObjectTT->setActive(false);
		std::cout << "recusrive_game_object_activeinactive_t is partially implemented" << std::endl;
	}
}

void fadeout_t::interact() {
	// TODO
	// target->setVolume(targetVolume);
	target->setEnabled(false);
}


// TODO: manage well known objects in the scene better
game_object_t* playa;


template<float maxDistance>
struct GroundRaycastCallback: public reactphysics3d::RaycastCallback {
	float distance = maxDistance;

	virtual float notifyRaycastHit(const reactphysics3d::RaycastInfo& raycastInfo) override {
		auto collider = (box_collider_t*)raycastInfo.collider->getUserData();
		auto distance = raycastInfo.hitFraction * maxDistance;
		if (collider->gameObject->isActive()) {
			this->distance = distance;
			return raycastInfo.hitFraction;
		} else {
			return -1;
		}
	}
};

void player_movement_t::update(float deltaTime) {
	// TODO: well known objects
	if (playa == nullptr) {
		playa = gameObject;
	}

	if (!canMove || !pavo_state_t::getEnv()->canMove) {
		return;
	}

	auto contMaple = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	if (!contMaple) {
		return;
	}
	auto state = (cont_state_t *)maple_dev_status(contMaple);
	if (!state) {
		return;
	}

	float movement = 0;
	if (state->a) {
		movement = 1;
	}
	if (state->y) {
		movement = -0.7f;
	}

	if (movement != 0) {
		GroundRaycastCallback<10.f> moveCheck;
	
		reactphysics3d::Vector3 playaPos = {playa->ltw.pos.x, playa->ltw.pos.y + 3.8f/2, playa->ltw.pos.z}; // TODO: the 3.8f should come from the character controller
		reactphysics3d::Vector3 forwardAt = { playa->ltw.at.x, playa->ltw.at.y, playa->ltw.at.z};
	
		if (movement < 0) {
			forwardAt.z = -forwardAt.z;
		}
	
		reactphysics3d::Ray ray(playaPos, playaPos + forwardAt * moveCheck.distance);
	
		physicsWorld->raycast(ray, &moveCheck);
	
		if (moveCheck.distance > 5) {
			movement *= speed * deltaTime;
	
		
			float movementX = sin(gameObject->rotation.y * deg2rad) * movement;
			float movementZ = cos(gameObject->rotation.y * deg2rad) * movement;
			gameObject->position.x += movementX;
			gameObject->position.z += movementZ;
		}

		static V3d lastPos = {-1000, -1000,-1000};
		// TODO: move to a better place
		if (lastPos.x != playa->ltw.pos.x || lastPos.y != playa->ltw.pos.y || lastPos.z != playa->ltw.pos.z) {
			lastPos = playa->ltw.pos;
			reactphysics3d::Vector3 downAt = { 0, -1, 0 };
			GroundRaycastCallback<10.f> groundCheck1, groundCheck2;
			

			reactphysics3d::Vector3 playaPos1 = {playa->ltw.pos.x+0.2f, playa->ltw.pos.y, playa->ltw.pos.z};
			reactphysics3d::Ray ray1(playaPos1, playaPos1 + downAt*groundCheck1.distance);
			physicsWorld->raycast(ray1, &groundCheck1);

			reactphysics3d::Vector3 playaPos2 = {playa->ltw.pos.x-0.2f, playa->ltw.pos.y, playa->ltw.pos.z};
			reactphysics3d::Ray ray2(playaPos2, playaPos2 + downAt*groundCheck2.distance);

			physicsWorld->raycast(ray2, &groundCheck2);

			float minGroundDistance = std::min(groundCheck1.distance, groundCheck2.distance);
		
			playa->position.y -= minGroundDistance - 0.5f - 3.8f/2; // TODO: the 3.8f should come from the character controller
		}
	}
}

template<float maxDistance>
struct LookAtCheck: public reactphysics3d::RaycastCallback {
	box_collider_t* collider = nullptr;
	float distance = 1000;

	virtual float notifyRaycastHit(const reactphysics3d::RaycastInfo& raycastInfo) override {
		auto collider = (box_collider_t*)raycastInfo.collider->getUserData();
		float distance = raycastInfo.hitFraction * maxDistance;
		if (collider->gameObject->isActive()) {
			this->collider = collider;
			this->distance = distance;
			return raycastInfo.hitFraction;
		} else {
			return -1;
		}
	}
};


interactable_t* mouse_look_t::inter;
pavo_interactable_t* mouse_look_t::ii2LookAt;

void mouse_look_t::update(float deltaTime) {
	auto contMaple = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	if (!contMaple) {
		return;
	}
	auto state = (cont_state_t *)maple_dev_status(contMaple);
	if (!state) {
		return;
	}

	static bool last_b = false;
	bool inspected = false;
	if (!last_b && state->b) {
		inspected = true;
	}
	last_b = state->b;

	static bool last_x = false;
	bool interacted = false;
	if (!last_x && state->x) {
		interacted = true;
	}
	last_x = state->x;

	if (lookEnabled && pavo_state_t::getEnv()->canLook) {
		if (rotateEnabled && pavo_state_t::getEnv()->canRotate) {
			gameObjects[playerBodyIndex]->rotation.y += (float)state->joyx * rotateSpeed * deltaTime;
			gameObject->rotation.x += (float)state->joyy * rotateSpeed * deltaTime;
			gameObject->rotation.x = std::clamp(gameObject->rotation.x, -89.0f, 89.0f);
		}

		if (pavo_state_t::getEnv()->onInteraction) {
			if (inspected) {
				auto oldHandler = pavo_state_t::getEnv()->onInteraction;
				pavo_state_t::getEnv()->onInteraction = nullptr;
				auto newHandler = oldHandler();

				if (newHandler) {
					pavo_state_t::getEnv()->onInteraction = newHandler;
				}
			}
		} else {
			bool Handled = false;
			LookAtCheck<50.f> lookAtChecker;

			reactphysics3d::Vector3 cameraPos = {gameObject->ltw.pos.x, gameObject->ltw.pos.y, gameObject->ltw.pos.z};
			reactphysics3d::Vector3 cameraAt = {gameObject->ltw.at.x, gameObject->ltw.at.y, gameObject->ltw.at.z};
			reactphysics3d::Ray ray(cameraPos, cameraPos + cameraAt*50);
			
			// physics is one step behind here
			physicsWorld->raycast(ray, &lookAtChecker);
		
			// lookAtChecker.finalize();

			if (lookAtChecker.collider) {
				pavo_interactable_t* ii2 = lookAtChecker.collider->gameObject->getComponent<pavo_interactable_t>();
				if (ii2 && ii2->getInteractionRadius() >= lookAtChecker.distance) {
					if (ii2LookAt != ii2)
					{
						if (ii2LookAt != nullptr)
						{
							ii2LookAt->lookAway();
						}
						ii2LookAt = ii2;
						ii2LookAt->lookAt();
					}

					if (inspected) {
						Handled = true;
						ii2->focused();
					} else if (interacted) {
						ii2->interact();
					}
				} else if (ii2LookAt != nullptr) {
					ii2LookAt->lookAway();
					ii2LookAt = nullptr;
				} else if (inspected) {
					if (auto temp = lookAtChecker.collider->gameObject->getComponent<interactable_t>())
					{
						if (temp->interactionRadius >= lookAtChecker.distance)
						{
							if (inter != nullptr) {
								// TODO
								//messaging.Unfocused(inter);
								messageSpeaker = nullptr;
								messageText = nullptr;
							}

							inter = temp;
							inter->focused();
							Handled = true;
						} 
					} else if (inter != nullptr) {
						if (inter != nullptr) {
							// TODO
							// messaging.Unfocused(inter);
							messageSpeaker = nullptr;
							messageText = nullptr;
						}
						
						// TODO
						// messaging.Unfocused(nullptr);
						messageSpeaker = nullptr;
						messageText = nullptr;
					
						inter = nullptr;
					}
				} else if (interacted) {
					if (auto temp = lookAtChecker.collider->gameObject->getComponent<interactable_t>())
					{
						if (temp->interactionRadius >= lookAtChecker.distance)
						{
							if (inter != nullptr) {
								// TODO
								// messaging.Unfocused(inter);
								messageSpeaker = nullptr;
								messageText = nullptr;
							}
							inter = temp;
							inter->interact();
						}
					}
					else if (inter != nullptr)
						{
							if (inter != nullptr) {
								// TODO
								// messaging.Unfocused(inter);
								messageSpeaker = nullptr;
								messageText = nullptr;
							}
					
							inter = nullptr;
						}
				}

				//  else {
				// 	// TODO
				// 	lookAtMessage = nullptr;
				// }
 			} else { // this is a fix VS C#
				if (ii2LookAt != nullptr) {
					ii2LookAt->lookAway();
					ii2LookAt = nullptr;
				}
				lookAtMessage = nullptr;
				// TODO
				//  else if (CurrentLookAtInteractable != nullptr)
				//  {
				// 	 HideLookAtText();
				//  }
			}
			if (inspected && !Handled)
			{
				// TODO
				// Draw lazer beam!
				// LazerBeam?.gameObject.SetActive(true);
			}
		}
	}

	#if defined(DEBUG_LOOKAT)
	pointedGameObject = nullptr;
	#endif
}

void interactable_message_t::update(game_object_t* mainCamera) {
	LookAtCheck<25.f> lookAtChecker;

	reactphysics3d::Vector3 cameraPos = {mainCamera->ltw.pos.x, mainCamera->ltw.pos.y, mainCamera->ltw.pos.z};
	reactphysics3d::Vector3 cameraAt = {mainCamera->ltw.at.x, mainCamera->ltw.at.y, mainCamera->ltw.at.z};
	reactphysics3d::Ray ray(cameraPos, cameraPos + cameraAt*25);
	
	// physics is one step behind here
	physicsWorld->raycast(ray, &lookAtChecker);
	/*
	TODO:

	if (messaging.Visible() || dialog.Visible() || password.Visible() || map.Visible())
	{
		textGameObject.SetActive(false);
		lastInVisible = Time.time;
	}
	// TODO: This is also a hack
	else if ((Time.time - lastInVisible) > 0.05f)
	{
		textGameObject.SetActive(true);
	}
	*/

	/*Interactable temp;
	if (wasHit && hit.collider.gameObject.TryGetComponent<Interactable>(out temp))
	{
		if (temp.InteractionRadious >= hit.distance)
		{
			if (messaging.busy != (object)temp)
			{
				ShowLookAtText(temp.LookAtMessage.Equals("") ? DefaultLookAtText : temp.LookAtMessage, temp);
   
			}
		}
		else
		{
			HideLookAtText();
		}
	}
	else if (CurrentLookAtInteractable != null)
	{
		HideLookAtText();
	}*/
	interactable_t* temp;
	if (lookAtChecker.collider != nullptr && (temp = lookAtChecker.collider->gameObject->getComponent<interactable_t>())) {
		if (temp->interactionRadius >= lookAtChecker.distance) {
			// TODO
			// if (messaging.busy != (object)temp)
			// {
			// 	ShowLookAtText(temp.LookAtMessage.Equals("") ? DefaultLookAtText : temp.LookAtMessage, temp);
			// }
			currentLookAtInteractable = temp;
			lookAtMessage = temp->lookAtMessage ? temp->lookAtMessage : defaultLookAtText;
		} else {
			// TODO
			// HideLookAtText();
			currentLookAtInteractable = nullptr;
			lookAtMessage = nullptr;
		}
	} else if (currentLookAtInteractable != nullptr) {
		// TODO
		// HideLookAtText();
		currentLookAtInteractable = nullptr;
		lookAtMessage = nullptr;
	}
}

void teleporter_t::update(float deltaTime) {
	if (!requiresTrigger) {
		tryTeleport();	
	}
	// TODO: fluctuate FOV
	// TODO: Fade
}

void teleporter_t::tryTeleport() {
	if (!playa || destinationIndex == SIZE_MAX) {
		return;
	}

	// (playa->position - gameobject->position)
	auto distance  = sqrtf(
		(playa->ltw.pos.x - gameObject->ltw.pos.x) * (playa->ltw.pos.x - gameObject->ltw.pos.x) +
		(playa->ltw.pos.y - gameObject->ltw.pos.y) * (playa->ltw.pos.y - gameObject->ltw.pos.y) +
		(playa->ltw.pos.z - gameObject->ltw.pos.z) * (playa->ltw.pos.z - gameObject->ltw.pos.z)
	);

	if (distance < radius) {
		if (requiresItem) {
			if (pavo_state_t::hasItem(requiresItem)) {
				if (removeAfterUnlock) {
					pavo_state_t::removeItem(requiresItem);
				}
			} else {
				return;
			}
		}
		// TODO: fluctuate FOV
		if (fade) {
			queueCoroutine(this->doFade(fadeInDuraiton, fadeOutDuration));
		} else {
			teleport();
		}
	}
}

RGBAf overlayImage;

Task teleporter_t::doFade(float fadeInDuration, float fadeOutDuration) {
	float currentTime = 0;
	float start = 0;

	// DisableMovement();
	player_movement_t::canMove = false;

	overlayImage.red = fadeColor[1];
	overlayImage.green = fadeColor[2];
	overlayImage.blue = fadeColor[3];

	while (currentTime < fadeInDuration) {
		currentTime += timeDeltaTime;
		overlayImage.alpha = std::lerp(start, 1, currentTime / fadeInDuration);
		co_yield Step::Frame;
	}

	overlayImage.alpha = 1;

	teleport();

	//EnableMovement();
	player_movement_t::canMove = true;

	currentTime = 0;
	start = overlayImage.alpha;
	while (currentTime < fadeOutDuration) {
		currentTime += timeDeltaTime;
		overlayImage.alpha = std::lerp(start, 0, currentTime / fadeOutDuration);
		co_yield Step::Frame;
	}
	overlayImage.alpha = 0;
}

void teleporter_t::teleport() {
	if (setPosition) {
		playa->position = { 
			gameObjects[destinationIndex]->ltw.pos.x,
			gameObjects[destinationIndex]->ltw.pos.y,
			gameObjects[destinationIndex]->ltw.pos.z
		};
	}
	if (setRotation) {
		// TODO this is wrong
		playa->rotation = gameObjects[destinationIndex]->rotation;
	}
}

void tv_programming_t::update(float deltaTime) {
	totalTime += deltaTime;
	gameObject->logical_submesh = int((size_t)(totalTime / materialRate) % materialCount);
	gameObject->materials[0] = ::materials[materials[gameObject->logical_submesh]];
}

void box_collider_t::update(float deltaTime) {
	if (!rigidBody) {
		reactphysics3d::Transform t;
		rigidBody = physicsWorld->createRigidBody(t);
		rigidBody->setUserData(this);
		rigidBody->setType(reactphysics3d::BodyType::STATIC);
		#if defined(DEBUG_PHYSICS)
		rigidBody->setIsDebugEnabled(true);
		#endif
	}

	matrix_t localOffset = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		center.x, center.y, center.z, 1
	};
	mat_load((matrix_t*)&gameObject->ltw);
	mat_apply(&localOffset);
	mat_store(&localOffset);
	
	reactphysics3d::Transform t;
	reactphysics3d::Vector3 scale3d;
	t.setFromOpenGL(&localOffset[0][0]);
	rigidBody->setTransform(t);

	if (boxShape == nullptr) {
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroyBoxShape(boxShape);
		}
		boxShape = physicsCommon.createBoxShape(reactphysics3d::Vector3(halfSize.x, halfSize.y, halfSize.z));
		collider = rigidBody->addCollider(boxShape, reactphysics3d::Transform::identity());
		collider->setUserData(this);
	}
}

void sphere_collider_t::update(float deltaTime) {
	if (!rigidBody) {
		reactphysics3d::Transform t;
		rigidBody = physicsWorld->createRigidBody(t);
		rigidBody->setUserData(this);
		rigidBody->setType(reactphysics3d::BodyType::STATIC);
		#if defined(DEBUG_PHYSICS)
		rigidBody->setIsDebugEnabled(true);
		#endif
	}

	matrix_t localOffset = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		center.x, center.y, center.z, 1
	};
	mat_load((matrix_t*)&gameObject->ltw);
	mat_apply(&localOffset);
	mat_store(&localOffset);

	reactphysics3d::Transform t;
	reactphysics3d::Vector3 scale3d;
	t.setFromOpenGL(&localOffset[0][0]);
	rigidBody->setTransform(t);

	if (sphereShape == nullptr) {
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroySphereShape(sphereShape);
		}
		sphereShape = physicsCommon.createSphereShape(radius);
		collider = rigidBody->addCollider(sphereShape, reactphysics3d::Transform::identity());
		collider->setUserData(this);
	}
}

void capsule_collider_t::update(float deltaTime) {
	if (!rigidBody) {
		reactphysics3d::Transform t;
		rigidBody = physicsWorld->createRigidBody(t);
		rigidBody->setUserData(this);
		rigidBody->setType(reactphysics3d::BodyType::STATIC);
		#if defined(DEBUG_PHYSICS)
		rigidBody->setIsDebugEnabled(true);
		#endif
	}

	matrix_t localOffset = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		center.x, center.y, center.z, 1
	};
	mat_load((matrix_t*)&gameObject->ltw);
	mat_apply(&localOffset);
	mat_store(&localOffset);

	reactphysics3d::Transform t;
	reactphysics3d::Vector3 scale3d;
	t.setFromOpenGL(&localOffset[0][0]);
	rigidBody->setTransform(t);
	
	if (capsuleShape == nullptr) {
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroyCapsuleShape(capsuleShape);
		}
		capsuleShape = physicsCommon.createCapsuleShape(radius, height);
		collider = rigidBody->addCollider(capsuleShape, reactphysics3d::Transform::identity());
		collider->setUserData(this);
	}
}

std::map<std::pair<float*, uint16_t*>, reactphysics3d::TriangleMesh*> mesh_collider_triangles;

void mesh_collider_t::update(float deltaTime) {
	if (!rigidBody) {
		reactphysics3d::Transform t;
		rigidBody = physicsWorld->createRigidBody(t);
		rigidBody->setUserData(this);
		rigidBody->setType(reactphysics3d::BodyType::STATIC);
		#if defined(DEBUG_PHYSICS)
		rigidBody->setIsDebugEnabled(true);
		#endif
	}

	reactphysics3d::Transform t;

	t.setFromOpenGL(&gameObject->ltw.m00);
	rigidBody->setTransform(t);

	
	if (meshShape == nullptr) {
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroyConcaveMeshShape(meshShape);
		}
		meshShape = physicsCommon.createConcaveMeshShape(vertices, bvh);
		collider = rigidBody->addCollider(meshShape, reactphysics3d::Transform::identity());
		collider->setUserData(this);
	}
}

reactphysics3d::PhysicsCommon physicsCommon;
reactphysics3d::PhysicsWorld* physicsWorld = nullptr;

extern font_t fonts_0;
extern font_t fonts_1;
extern font_t fonts_19;

void measureText(font_t* font, float em, const char* text, float* width, float* height) {
	auto len = strlen(text);
	float x = 0;
	float y = 0;
	float scale = 0.4f;

	*width = 0;
	*height = 0;
	for (size_t i = 0; i < len; i++) {
		auto c = text[i];
		if (c == '\n') {
			*width = std::max(*width, x);
			x = 0;
			y += em;
			continue;
		}
		int glyphId = -1;
		for (int j = 0; j < FONT_CHAR_COUNT; j++) {
			if (fontChars[j] == c) {
				glyphId = j;
				break;
			}
		}
		if (glyphId == -1) {
			x += em;
			continue;
		}
		auto glyph = &font->chars[glyphId];
		
		x += glyph->advance * scale;
	}

	*width = std::max(*width, x);
	*height = std::max(*height, y + em);
}

void drawText(font_t* font, float em, float x, float y, const char* text, float a, float r, float g, float b, float resetx = 0) {

	float scale = 0.4f;

	pvr_poly_hdr_t hdr;
	pvr_poly_cxt_txr_fast(
		&hdr,
		PVR_LIST_TR_POLY,

		font->texture->flags,
		font->texture->lw,
		font->texture->lh,
		(uint8_t*)font->texture->data - font->texture->offs,

		PVR_FILTER_BILINEAR,

		// flip_u, clamp_u, flip_v, clamp_v,
		PVR_UVFLIP_NONE,
		PVR_UVCLAMP_NONE,
		PVR_UVFLIP_NONE,
		PVR_UVCLAMP_NONE,
		PVR_UVFMT_16BIT,

		PVR_CLRFMT_4FLOATS,
		PVR_BLEND_SRCALPHA,
		PVR_BLEND_INVSRCALPHA,
		PVR_DEPTHCMP_ALWAYS,
		PVR_DEPTHWRITE_DISABLE,
		PVR_CULLING_NONE,
		PVR_FOG_DISABLE
	);

	pvr_prim(&hdr, sizeof(hdr));

	y += em;
	auto len = strlen(text);
	for (size_t i = 0; i < len; i++) {
		auto c = text[i];
		if (c == '\n') {
			x = resetx;
			y += em;
			continue;
		}
		int glyphId = -1;
		for (int j = 0; j < FONT_CHAR_COUNT; j++) {
			if (fontChars[j] == c) {
				glyphId = j;
				break;
			}
		}
		if (glyphId == -1) {
			x += em;
			continue;
		}
		auto glyph = &font->chars[glyphId];
		
		pvr_vertex64_t vtx;

		vtx.flags = PVR_CMD_VERTEX;
		vtx.x = x + glyph->x1*scale;
		vtx.y = y - glyph->y1*scale;
		vtx.z = 1.0f;
		vtx.u = float16(glyph->u0).raw;
		vtx.v = float16(1 - glyph->v0).raw;
		vtx.a = a; vtx.r = r; vtx.g = g; vtx.b = b;
		pvr_prim(&vtx, sizeof(vtx));

		vtx.flags = PVR_CMD_VERTEX;
		vtx.x = x + glyph->x0*scale;
		vtx.y = y - glyph->y1*scale;
		vtx.z = 1.0f;
		vtx.u = float16(glyph->u1).raw;
		vtx.v = float16(1 - glyph->v1).raw;
		vtx.a = a; vtx.r = r; vtx.g = g; vtx.b = b;
		pvr_prim(&vtx, sizeof(vtx));

		vtx.flags = PVR_CMD_VERTEX;
		vtx.x = x + glyph->x1*scale;
		vtx.y = y - glyph->y0*scale;
		vtx.z = 1.0f;
		vtx.u = float16(glyph->u3).raw;
		vtx.v = float16(1 - glyph->v3).raw;
		vtx.a = a; vtx.r = r; vtx.g = g; vtx.b = b;
		pvr_prim(&vtx, sizeof(vtx));

		vtx.flags = PVR_CMD_VERTEX_EOL;
		vtx.x = x + glyph->x0*scale;
		vtx.y = y - glyph->y0*scale;
		vtx.z = 1.0f;
		vtx.u = float16(glyph->u2).raw;
		vtx.v = float16(1 - glyph->v2).raw;
		vtx.a = a; vtx.r = r; vtx.g = g; vtx.b = b;
		pvr_prim(&vtx, sizeof(vtx));

		x += glyph->advance*scale;
	}
}

void drawTextCentered(font_t* font, float em, float x, float y, const char* text, float a, float r, float g, float b) {
	float width, height;
	measureText(font, em, text, &width, &height);
	x -= width / 2;
	y -= height / 2;
	drawText(font, em, x, y, text, a, r, g, b, x);
}

void drawTextRightBottom(font_t* font, float em, float x, float y, const char* text, float a, float r, float g, float b) {
	float width, height;
	measureText(font, em, text, &width, &height);
	x -= width;
	y -= height;
	drawText(font, em, x, y, text, a, r, g, b, x);
}

void drawTextLeftBottom(font_t* font, float em, float x, float y, const char* text, float a, float r, float g, float b) {
	float width, height;
	measureText(font, em, text, &width, &height);
	y -= height;
	drawText(font, em, x, y, text, a, r, g, b, x);
}

std::list<Task> coroutines;
void queueCoroutine(Task&& coroutine) {
	coroutine.next();
	if (!coroutine.done()) {
		coroutines.push_back(std::move(coroutine));
	}
}

// TODO: move to some header
void InitializeFlowMachines();
void InitializeAudioClips();
void InitializeAudioSources();

void positionUpdate() {
	for (auto go: gameObjects) {
		if (!go->isActive()) {
			go->meshSphere.radius = 0;
			continue;
		}

		r_matrix_t pos_mtx = {
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			go->position.x, go->position.y, go->position.z, 1
		};

		r_matrix_t rot_mtx_x = {
			1, 0, 0, 0,
			0, cosf(go->rotation.x*deg2rad), sinf(go->rotation.x*deg2rad), 0,
			0, -sinf(go->rotation.x*deg2rad), cosf(go->rotation.x*deg2rad), 0,
			0, 0, 0, 1
		};
		r_matrix_t rot_mtx_y = {
			cosf(go->rotation.y*deg2rad), 0, -sinf(go->rotation.y*deg2rad), 0,
			0, 1, 0, 0,
			sinf(go->rotation.y*deg2rad), 0, cosf(go->rotation.y*deg2rad), 0,
			0, 0, 0, 1
		};
		r_matrix_t rot_mtx_z = {
			cosf(go->rotation.z*deg2rad), sinf(go->rotation.z*deg2rad), 0, 0,
			-sinf(go->rotation.z*deg2rad), cosf(go->rotation.z*deg2rad), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		};
		r_matrix_t scale_mtx = {
			go->scale.x, 0, 0, 0,
			0, go->scale.y, 0, 0,
			0, 0, go->scale.z, 0,
			0, 0, 0, 1
		};
		static r_matrix_t identity_mtx = {
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		};


		if (go->parent) {
			mat_load((matrix_t*)&go->parent->ltw);
			mat_apply((matrix_t*)&pos_mtx);
		} else {
			mat_load((matrix_t*)&pos_mtx);
		}

		mat_apply((matrix_t*)&rot_mtx_y);
		mat_apply((matrix_t*)&rot_mtx_x);
		mat_apply((matrix_t*)&rot_mtx_z);
		mat_apply((matrix_t*)&scale_mtx);
		mat_store((matrix_t*)&go->ltw);
		go->maxWorldScale = GetMaxScale(go->ltw);
		if (go->mesh && go->mesh_enabled) {
			Sphere &sphere = go->mesh->bounding_sphere;
			float w;
			mat_trans_nodiv_nomod(sphere.center.x, sphere.center.y, sphere.center.z, go->meshSphere.center.x, go->meshSphere.center.y, go->meshSphere.center.z, w);
			(void)w;
			go->meshSphere.radius = sphere.radius * go->maxWorldScale;
		} else {
			go->meshSphere.radius = 0;
		}
	}
}

void physicsUpdate(float deltaTime) {
	// physics (these use ltw)
	for (auto box_collider = box_colliders; *box_collider; box_collider++) {
		if ((*box_collider)->gameObject->isActive()) {
			(*box_collider)->update(deltaTime);
		}
	}
	for (auto sphere_collider = sphere_colliders; *sphere_collider; sphere_collider++) {
		if ((*sphere_collider)->gameObject->isActive()) {
			(*sphere_collider)->update(deltaTime);
		}
	}
	for (auto capsule_collider = capsule_colliders; *capsule_collider; capsule_collider++) {
		if ((*capsule_collider)->gameObject->isActive()) {
			(*capsule_collider)->update(deltaTime);
		}
	}
	for (auto mesh_collider = mesh_colliders; *mesh_collider; mesh_collider++) {
		if ((*mesh_collider)->gameObject->isActive()) {
			(*mesh_collider)->update(deltaTime);
		}
	}
	
	physicsWorld->update(deltaTime);
}

#if defined(DEBUG_PHYSICS)
bool drawphys;
#else
constexpr bool drawphys = false;
#endif

inline Sphere mergeSpheres(const Sphere* A, const Sphere* B) {
    // --- 1) ignore any degenerate (zero‑radius) sphere ---
    if (A->radius == 0.0f) return *B;
    if (B->radius == 0.0f) return *A;

    // --- 2) now both A and B have radius > 0, do the normal merge ---
    V3d d    = sub(B->center,   A->center);
    float dist = length(d);

    // one already contains the other?
    if (A->radius >= dist + B->radius) return *A;
    if (B->radius >= dist + A->radius) return *B;

    // otherwise build the minimal enclosing sphere
    float Rm = (dist + A->radius + B->radius) * 0.5f;
    float K  = (Rm - A->radius) / dist;
    // new center = A->center + K * (B->center - A->center)
    V3d  Cm = add(A->center, scale(d, K));

    return Sphere{ Cm, Rm };
}

void mergeChildSpheresAndFrustum(camera_t* cam, game_object_t* go) {
	if (!go->isActive()) {
		go->compoundSphere.radius = 0;
		go->compoundVisible = false;
		return;
	}

	go->compoundSphere = go->meshSphere;
	auto childNum = go->children;
	while(*childNum != SIZE_MAX) {
		auto child = gameObjects[*childNum++];
		mergeChildSpheresAndFrustum(cam, child);
		go->compoundSphere = mergeSpheres(&go->compoundSphere, &child->compoundSphere);
	}

	go->compoundVisible = cam->frustumTestSphere(&go->compoundSphere) != camera_t::SPHEREOUTSIDE;
}

template<int mode>
void renderSelfAndChildren(camera_t* cam, game_object_t* go) {
	if (!go->compoundVisible) return;

	if (go->mesh_enabled && go->mesh && go->materials) {
		if (mode == 0 && go->materials[0]->color.alpha == 1) {
			renderQuads(cam, go);
		}
		bool hasAnyOpaque = false;
		bool hasAnyTransp = false;
		for (int subM = 0; subM < go->submesh_count; subM++) {
			bool hasAlpha = go->materials[subM]->hasAlpha();
			hasAnyOpaque |= !hasAlpha;
			hasAnyTransp |= hasAlpha;
		}
		if (mode == 1 && hasAnyOpaque) {
			renderMesh<PVR_LIST_OP_POLY>(cam, go);
		}
		if (mode == 2 && hasAnyTransp) {
			renderMesh<PVR_LIST_TR_POLY>(cam, go);
		}
	}
	auto childNum = go->children;
	while(*childNum != SIZE_MAX) {
		auto child = gameObjects[*childNum++];
		renderSelfAndChildren<mode>(cam, child);
	}
}

static vec3f skybox_face_verts[6][4] = {
	/* +X */ {{ 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1}},
	/* -X */ {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}},
	/* +Y */ {{-1, 1,-1},{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1}},
	/* -Y */ {{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1},{-1,-1,-1}},
	/* +Z */ {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},
	/* -Z */ {{ 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1}},
};

/* simple (0,0)->(1,1) UVs for each quad */
static const float skybox_face_uvs[4][2] = {
	{0.0f, 1.0f},
	{1.0f, 1.0f},
	{1.0f, 0.0f},
	{0.0f, 0.0f},
};

static int skybox_face_tex_remap[6] = { 5, 4, 2, 3, 0, 1};
typedef struct {
    float x,y,z,w;
    float u,v;
} skyvert_t;
static int clip_against_near(const skyvert_t *in, int in_cnt,
							 skyvert_t *out)
{
	int out_cnt = 0;
	for (int i = 0; i < in_cnt; ++i)
	{
		const skyvert_t *A = &in[i];
		const skyvert_t *B = &in[(i + 1) % in_cnt];
		float da = A->z + A->w;
		float db = B->z + B->w;

		// keep A if it's inside
		if (da >= 0.0f)
		{
			out[out_cnt++] = *A;
		}
		// if edge crosses, find intersection, but only if denom != 0
		if ((da >= 0.0f) ^ (db >= 0.0f))
		{
			float denom = da - db;
			if (fabsf(denom) > 1e-6f)
			{
				float t = da / denom;
				if (t > 0.0f && t < 1.0f)
				{
					skyvert_t I;
					I.x = A->x + t * (B->x - A->x);
					I.y = A->y + t * (B->y - A->y);
					I.z = A->z + t * (B->z - A->z);
					I.w = A->w + t * (B->w - A->w);
					I.u = A->u + t * (B->u - A->u);
					I.v = A->v + t * (B->v - A->v);
					out[out_cnt++] = I;
				}
			}
		}
	}
	return out_cnt;
}
static void render_skybox(camera_t* camera) {
    /* intermediate buffers */
    skyvert_t tmp_in[4], tmp_out[8];
    vector_t transformed[24];
    pvr_dr_state_t dr_state;
    pvr_poly_hdr_t   hdr;

	pvr_dr_init(&dr_state);

    /* 1) build clip-space verts */
    mat_load(&camera->devViewProjScreenSkybox);

	vec3f* face = skybox_face_verts[0];
	vector_t* xformed = transformed;
    for (unsigned i = 24; i != 0; i--) {
		mat_trans_nodiv_nomod(face->x, face->y, face->z, xformed->x, xformed->y, xformed->z, xformed->w);
		face++;
		xformed++;
	}

    for(int f = 0; f < 6; ++f) {
        for(int j = 0; j < 4; ++j) {
            vector_t *v4 = &transformed[f*4 + j];
            tmp_in[j].x = v4->x;  tmp_in[j].y = v4->y;
            tmp_in[j].z = v4->z;  tmp_in[j].w = v4->w;
            tmp_in[j].u = skybox_face_uvs[j][0];
            tmp_in[j].v = skybox_face_uvs[j][1];
        }

        int nverts = clip_against_near(tmp_in, 4, tmp_out);
        if(nverts < 3) continue;

		auto tex = skybox[skybox_face_tex_remap[f]];

        pvr_poly_cxt_txr_fast(
            &hdr,
            PVR_LIST_OP_POLY,

            tex->flags,
            tex->lw,
            tex->lh,
            (uint8_t*)tex->data - tex->offs,

            PVR_FILTER_BILINEAR,
            PVR_UVFLIP_NONE, PVR_UVCLAMP_U,
            PVR_UVFLIP_NONE, PVR_UVCLAMP_V,
            PVR_UVFMT_32BIT,
            PVR_CLRFMT_4FLOATS,
            PVR_BLEND_ONE, PVR_BLEND_ZERO,
            PVR_DEPTHCMP_ALWAYS, PVR_DEPTHWRITE_DISABLE,
            PVR_CULLING_NONE, PVR_FOG_DISABLE
        );

		pvr_prim(&hdr, sizeof(hdr));
        // triangulate as a fan, but emit each triangle as a 3-vert strip
        for(int t = 1; t < nverts - 1; ++t) {
            skyvert_t *tri[3] = {
                &tmp_out[0],
                &tmp_out[t],
                &tmp_out[t + 1]
            };
            for(int k = 0; k < 3; ++k) {
                pvr_vertex64_t *pv = (pvr_vertex64_t *)pvr_dr_target(dr_state);
                pv->flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                float iw = 1.0f / tri[k]->w;
                pv->x    = tri[k]->x * iw;
                pv->y    = tri[k]->y * iw;
                pv->z    = iw;
                pv->uf32 = tri[k]->u;
                pv->vf32 = tri[k]->v;
                pvr_dr_commit(pv);

                pv->a = skyboxTint.alpha;
				pv->r = skyboxTint.red;
				pv->g = skyboxTint.green;
				pv->b = skyboxTint.blue;

				pvr_dr_commit(reinterpret_cast<uint8_t*>(pv) + 32);
            }
        }
    }
}

void bakeLights() {
	for (auto gameObject: gameObjects) {
		if (!gameObject->mesh || !gameObject->materials) {
			continue;
		}

		// bakedColors
		const MeshInfo* meshInfo = (const MeshInfo*)&gameObject->mesh->data[0];
		uint8_t* colors = nullptr;
		std::vector<size_t> submesh_colors;
		size_t colorsSize = 0;

		for (size_t submesh_num = 0; submesh_num < (gameObject->submesh_count /*+ gameObject->logical_submesh*/); submesh_num++) {
			if (!gameObject->materials[submesh_num]) {
				// huh?
				break;
			}
			auto meshletInfoBytes = &gameObject->mesh->data[meshInfo[submesh_num].meshletOffset];
			bool textured = gameObject->materials[submesh_num]->texture != nullptr;
			
			submesh_colors.push_back(colorsSize);
			for (int16_t meshletNum = 0; meshletNum < meshInfo[submesh_num].meshletCount; meshletNum++) {
				auto meshlet = (const MeshletInfo*)meshletInfoBytes;
				meshletInfoBytes += sizeof(MeshletInfo) - 8 ; // (skin ? 0 : 8);
				
				Sphere sphere = meshlet->boundingSphere;
				mat_load((matrix_t*)&gameObject->ltw);
				float w;
				mat_trans_nodiv_nomod(sphere.center.x, sphere.center.y, sphere.center.z, sphere.center.x, sphere.center.y, sphere.center.z, w);
				(void)w;
				sphere.radius *= gameObject->maxWorldScale;
				
				unsigned selector = meshlet->flags;

				dce_set_mat_decode(
					meshlet->boundingSphere.radius / 32767.0f,
					meshlet->boundingSphere.center.x,
					meshlet->boundingSphere.center.y,
					meshlet->boundingSphere.center.z
				);
				auto colorsBase = colorsSize;
				colorsSize += meshlet->vertexCount * 3;
				colors = (uint8_t*)realloc(colors, colorsSize);
				memset(&colors[colorsBase], 0, meshlet->vertexCount * 3);

				{
					unsigned normalOffset = (selector & 8) ? (3 * 2) : (3 * 4);
					if (selector & 16) {
						normalOffset += 1 * 2;
					}

					normalOffset += (selector & 32) ? 2 : 4;

					auto normalPointer = &gameObject->mesh->data[meshlet->vertexOffset] + normalOffset;
					auto vtxSize = meshlet->vertexSize;

					unsigned smallSelector = ((selector & 8) ? 1 : 0);

					mat_load((matrix_t*)&gameObject->ltw);
					if (selector & 8) {
						// mat_load(&mtx);
						mat_apply(&DCE_MESHLET_MAT_DECODE);
					} else {
						// mat_load(&mtx);
					}

					matrix_t mtx;
					mat_store(&mtx);

					for (auto point = point_lights; *point; point++) {
						/*if ((*point)->gameObject->isActive())*/ {
							float dist = length(sub((*point)->gameObject->ltw.pos, sphere.center));
							float maxDist = sphere.radius + (*point)->Range;
							if (dist < maxDist) {
								tnlMeshletPointColorBakeSelector[smallSelector](&gameObject->mesh->data[meshlet->vertexOffset], &colors[colorsBase], normalPointer, meshlet->vertexCount, vtxSize, *point, &mtx, (matrix_t*)&gameObject->ltw);
							}
						}
					}
				}
			}
		}
	
		gameObject->bakedColors =(int8_t**)malloc(submesh_colors.size()*sizeof(int8_t*));
		for (size_t i = 0; i < submesh_colors.size(); i++) {
			gameObject->bakedColors[i] = (int8_t*)colors + submesh_colors[i];
		}
		
		for (size_t i = 0; i < colorsSize; i++) {
			colors[i] = colors[i] ^ 128;
		}
	}
}

int main(int argc, const char** argv) {

    if (pvr_params.fsaa_enabled) {
		pvr_params.vertex_buf_size = (1024 + 768) * 1024;
		pvr_params.opb_overflow_count = 4; // 307200 bytes
	} else {
		pvr_params.vertex_buf_size = (1024 + 1024) * 1024;
		pvr_params.opb_overflow_count = 7; // 268800 bytes
	}

	// if (videoModes[VIDEO_MODE].depth == 24) {
	// 	pvr_params.vertex_buf_size -= 128 * 1024;
	// 	pvr_params.opb_overflow_count -= pvr_params.fsaa_enabled ? 1 : 2;
	// }

    #if !defined(DC_SIM)
	// vid_set_mode(DM_640x480, videoModes[VIDEO_MODE].depth == 24 ? PM_RGB888P : PM_RGB565);
	#endif
    pvr_init(&pvr_params);

    #if defined(DC_SH4)
	chdir("/cd");
    #else
	chdir("repack-data/tlj");
    #endif

	loadScene("dream.ndt");

    InitializeHierarchy(gameObjects);
	InitializeComponents(gameObjects);
	InitializeFonts();
	InitializeFlowMachines();
	InitializeAudioClips();
	InitializeAudioSources();


    unsigned currentStamp = 0;

	#if defined(DC_SH4)
	OCR_SPACE = (uint8_t*)0x92000000;

	bool has_oix = true;
	enter_oix();
	*(volatile uint8_t*)OCR_SPACE = 1;
	if (*(volatile uint8_t*)OCR_SPACE != 1) {
		has_oix = false;
	}
	leave_oix();

	if (!has_oix) {
		dbglog(DBG_CRITICAL, "You appear to be using an emulator that does not support OIX. Attempting fallback to OCR\n");
		OCR_SPACE = (uint8_t*)0x7c001000;
		enter_oix = (void(*)())(((uintptr_t)&enter_ocr_) - 0x8c000000 + 0xAc000000);
		leave_oix = (void(*)())(((uintptr_t)&leave_ocr_) - 0x8c000000 + 0xAc000000);

		for (size_t i = 0; i < ARRAY_SIZE(submitMeshletSelector); i++) {
			submitMeshletSelector[i] = submitMeshletSelectorFallback[i];
		}

		for (size_t i = 0; i < ARRAY_SIZE(clipAndsubmitMeshletSelector); i++) {
			clipAndsubmitMeshletSelector[i] = clipAndsubmitMeshletSelectorFallback[i];
		}
	}
	#endif

	physicsWorld = physicsCommon.createPhysicsWorld();
	pavo_state_t::Initialize({
		.paused = false,
		.canMove = true,
		.canRotate = true,
		.canLook = true,
		.onInteraction = nullptr,
		.cursorVisible = false,
		.gameMode = pgm_Ingame
	});

	#if defined(DEBUG_PHYSICS)
	physicsWorld->setIsDebugRenderingEnabled(true);
	reactphysics3d::DebugRenderer& debugRenderer = physicsWorld->getDebugRenderer();
	debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLIDER_AABB, true);
	//debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
	#endif

	// initial positions
	positionUpdate();
	physicsUpdate(0.01);

	bakeLights();

	for (auto go: gameObjects) {
		if (go->isActive()) {
			audio_source_t** audioSources = go->getComponents<audio_source_t>();

			if (audioSources) {
				do {
					auto audioSource = *audioSources;
					if (audioSource->enabled){
						audioSource->awake();
					}
				} while(*++audioSources);
			}
		}
	}

    auto tp_last_frame = std::chrono::system_clock::now() - std::chrono::milliseconds(16);
    for(;;) {
        currentStamp++;
        auto tp_this_frame = std::chrono::system_clock::now();
        
        auto deltaTime = std::chrono::duration_cast<std::chrono::duration<float>>(tp_this_frame - tp_last_frame).count();
        tp_last_frame = tp_this_frame;
        // get input
        auto contMaple = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (contMaple) {
            auto state = (cont_state_t *)maple_dev_status(contMaple);

            if (state) {
                if (state->start) {
                    break;
                }

				if (state->ltrig > 64) {
					deltaTime *= state->ltrig / 25.f;
				}
            }
        }

		timeDeltaTime = deltaTime;


		// components
        for(auto& animator: animators) {
			if (gameObjects[animator->gameObjectIndex]->isActive()) {
				animator->update(deltaTime);
			}
        }

		for (auto camera = cameras; *camera; camera++) {
			if ((*camera)->gameObject->isActive()) {
				// (*camera)->update(deltaTime);
			}
		}

		// scripts
		for (auto proximity_interactable = proximity_interactables; *proximity_interactable; proximity_interactable++) {
			if ((*proximity_interactable)->gameObject->isActive()) {
				(*proximity_interactable)->update(deltaTime);
			}
		}
		for (auto mouse_look = mouse_looks; *mouse_look; mouse_look++) {
			if ((*mouse_look)->gameObject->isActive()) {
				(*mouse_look)->update(deltaTime);
			}
		}
		for (auto player_movement = player_movements; *player_movement; player_movement++) {
			if ((*player_movement)->gameObject->isActive()) {
				(*player_movement)->update(deltaTime);
			}
		}
		for (auto teleporter = teleporters; *teleporter; teleporter++) {
			if ((*teleporter)->gameObject->isActive()) {
				(*teleporter)->update(deltaTime);
			}
		}

		// interactions
		for (auto game_object_activeinactive = game_object_activeinactives; *game_object_activeinactive; game_object_activeinactive++) {
			if ((*game_object_activeinactive)->gameObject->isActive()) {
				//(*game_object_activeinactive)->update(deltaTime);
			}
		}
		for (auto timed_activeinactive = timed_activeinactives; *timed_activeinactive; timed_activeinactive++) {
			if ((*timed_activeinactive)->gameObject->isActive()) {
				(*timed_activeinactive)->update(deltaTime);
			}
		}
		for (auto fadein = fadeins; *fadein; fadein++) {
			if ((*fadein)->gameObject->isActive()) {
				//(*fadein)->update(deltaTime);
			}
		}
		for (auto show_message = show_messages; *show_message; show_message++) {
			if ((*show_message)->gameObject->isActive()) {
				(*show_message)->update(deltaTime);
			}
		}
		for (auto teleporter_trigger = teleporter_triggers; *teleporter_trigger; teleporter_trigger++) {
			if ((*teleporter_trigger)->gameObject->isActive()) {
				// (*teleporter_trigger)->update(deltaTime);
			}
		}

		// zoom in out interactions
		for (auto zoom_in_out = zoom_in_outs; *zoom_in_out; zoom_in_out++) {
			if ((*zoom_in_out)->gameObject->isActive()) {
				// (*zoom_in_out)->update(deltaTime);
			}
		}

		for (auto play_sound = play_sounds; *play_sound; play_sound++) {
			if ((*play_sound)->gameObject->isActive()) {
				// (*play_sound)->update(deltaTime);
			}
		}

		for (auto tv_programming = tv_programmings; *tv_programming; tv_programming++) {
			if ((*tv_programming)->gameObject->isActive()) {
				(*tv_programming)->update(deltaTime);
			}
		}

		

		// ugly hack: coroutines are after position set

		positionUpdate();

		// coroutines
		for (auto coroutine = coroutines.begin(); coroutine != coroutines.end(); ) {
			coroutine->next();
			if (coroutine->done()) {
				coroutine = coroutines.erase(coroutine);
			} else {
				coroutine++;
			}
		}

		physicsUpdate(deltaTime);

		// find current camera
		camera_t* currentCamera = nullptr;
		for (auto camera = cameras; *camera; camera++) {
			if ((*camera)->gameObject->isActive()) {
				currentCamera = *camera;
				break;
			}
		}

		if (!currentCamera) {
			std::cout << "No active camera found" << std::endl;
			continue;
		}

		for (auto interactable_message = interactable_messages; *interactable_message; interactable_message++) {
			if ((*interactable_message)->gameObject->isActive()) {
				(*interactable_message)->update(currentCamera->gameObject);
			}
		}

		// ANOTHER HACK: audio sources
		for (auto audio_source = audio_sources; *audio_source; audio_source++) {
			if ((*audio_source)->gameObject->isActive()) {
				(*audio_source)->update(playa);
			}
		}

        currentCamera->beforeRender(4.0f / 3.0f);

		auto rootNum = roots;
		do {
			mergeChildSpheresAndFrustum(currentCamera, gameObjects[*rootNum++]);
		} while(*rootNum != SIZE_MAX);

		// render frame
		memset(zBuffer, 0, sizeof(zBuffer));
		
		enter_oix(); // renderSelfAndChildren<0> uses OCR_BUFFER for temps

		rootNum = roots;
		do {
			renderSelfAndChildren<0>(currentCamera, gameObjects[*rootNum++]);
		} while(*rootNum != SIZE_MAX);

		#if defined(DC_SIM)
		uint8_t pixBuffer[32][32];
		for (int y = 0; y < 32; y++) {
			for (int x = 0; x < 32; x++) {
				float v = zBuffer[y][x] * 3 * 255;
				if (v > 255) v = 255;
				if (v != 0 && v < 32) v = 32;
				pixBuffer[y][x] = v;
			}
		}
		stbi_write_bmp("/home/skmp/projects/dcue/build/zbuffer.bmp",32,32,1,pixBuffer);
		#endif

        pvr_set_zclip(0.0f);
        pvr_set_bg_color(0.5f, 0.5f, 0.5f);
		pvr_wait_ready();
		
		#if defined(DC_SIM)
		total_idx = 0;
		#endif

        pvr_scene_begin();
        pvr_dr_init(&drState);
        pvr_list_begin(PVR_LIST_OP_POLY);

		render_skybox(currentCamera);

		rootNum = roots;
		do {
			renderSelfAndChildren<1>(currentCamera, gameObjects[*rootNum++]);
		} while(*rootNum != SIZE_MAX);
		
        pvr_list_finish();
		
        pvr_dr_init(&drState);
        pvr_list_begin(PVR_LIST_TR_POLY);
		rootNum = roots;
		do {
			renderSelfAndChildren<2>(currentCamera, gameObjects[*rootNum++]);
		} while(*rootNum != SIZE_MAX);
		
		#if defined(DC_SIM)
		std::cout << total_idx << std::endl;
		#endif

		if (overlayImage.alpha != 0) {
			// draw a full screen quad
			pvr_poly_hdr_t hdr;
			pvr_poly_cxt_col_fast(
				&hdr,
				PVR_LIST_TR_POLY,
				PVR_CLRFMT_4FLOATS,
				PVR_BLEND_SRCALPHA,
				PVR_BLEND_INVSRCALPHA,
				PVR_DEPTHCMP_ALWAYS,
				PVR_DEPTHWRITE_DISABLE,
				PVR_CULLING_NONE,
				PVR_FOG_DISABLE
			);
			pvr_prim(&hdr, sizeof(hdr));


			pvr_vertex32_ut vtx[4];
			vtx[0].flags = PVR_CMD_VERTEX;
			vtx[0].x = 0;
			vtx[0].y = 0;
			vtx[0].z = 1;
			vtx[0].a = overlayImage.alpha;
			vtx[0].r = overlayImage.red;
			vtx[0].g = overlayImage.green;
			vtx[0].b = overlayImage.blue;

			vtx[1].flags = PVR_CMD_VERTEX;
			vtx[1].x = 0;
			vtx[1].y = 480;
			vtx[1].z = 1;
			vtx[1].a = overlayImage.alpha;
			vtx[1].r = overlayImage.red;
			vtx[1].g = overlayImage.green;
			vtx[1].b = overlayImage.blue;

			// Extend to generate a triangle strip
			vtx[2].flags = PVR_CMD_VERTEX;
			vtx[2].x = 640;
			vtx[2].y = 0;
			vtx[2].z = 1;
			vtx[2].a = overlayImage.alpha;
			vtx[2].r = overlayImage.red;
			vtx[2].g = overlayImage.green;
			vtx[2].b = overlayImage.blue;

			vtx[3].flags = PVR_CMD_VERTEX_EOL; // End of triangle strip
			vtx[3].x = 640;
			vtx[3].y = 480;
			vtx[3].z = 1;
			vtx[3].a = overlayImage.alpha;
			vtx[3].r = overlayImage.red;
			vtx[3].g = overlayImage.green;
			vtx[3].b = overlayImage.blue;

			pvr_prim(vtx, sizeof(pvr_vertex32_ut) * 4);
		}

		if (choices_options) {
			// TODO: don't recalc every frame
			int choices_count = 0;
			while(choices_options[++choices_count])
				;
			
			int drawY = 440;
			for (int choice = choices_count-1; choice >= 0; choice--) {
				bool current = choice_current == choice;
				drawTextLeftBottom(&fonts_0, 15, 40, drawY, choices_options[choice], 1, !current, 1, !current);
				drawY -= 18;
			}
			if (choices_prompt) {
				drawTextLeftBottom(&fonts_1, 20, 40, drawY, choices_prompt, 1, 1, 0.1, 0.1);
			}

			auto contMaple = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
			if (contMaple) {
				auto state = (cont_state_t *)maple_dev_status(contMaple);
				if (state) {
					static bool old_down = !state->dpad_down;
					if (!old_down && state->dpad_down && choice_current < (choices_count-1)) {
						choice_current++;
					}
					old_down = state->dpad_down;
					static bool old_up = !state->dpad_up;
					if (!old_up && state->dpad_up && choice_current > 0) {
						choice_current--;
					}
					old_up = state->dpad_up;

					if (state->a) {
						choice_chosen = choice_current;
					}
				}
			}
		} else if (messageText) {
			if (messageSpeaker) {
				drawTextLeftBottom(&fonts_1, 20, 40, 390, messageSpeaker, 1, 1, 1, 1);
			}
			drawTextLeftBottom(&fonts_0, 15, 40, 440, messageText, 1, 1, 1, 1);
		} else if (lookAtMessage) {
			drawTextCentered(&fonts_19, 40, 320, 240, lookAtMessage, 1, 1, 0.1, 0.1);
		}
		#if defined(DEBUG_PHYSICS)
		if (drawphys) {
			pvr_poly_hdr_t hdr;
			pvr_poly_cxt_col_fast(
				&hdr,
				PVR_LIST_TR_POLY,
				PVR_CLRFMT_4FLOATS,
				PVR_BLEND_SRCALPHA,
				PVR_BLEND_INVSRCALPHA,
				PVR_DEPTHCMP_GEQUAL,
				PVR_DEPTHWRITE_ENABLE,
				PVR_CULLING_NONE,
				PVR_FOG_DISABLE
			);
			pvr_prim(&hdr, sizeof(hdr));

			mat_load(&currentCamera->devViewProjScreen);
			for (int lineNum = 0; lineNum < debugRenderer.getNbLines(); lineNum++) {
				auto lines = debugRenderer.getLinesArray();

				auto line = lines[lineNum];

				float ignored;
				pvr_vertex32_ut vtx[4];
				vtx[0].flags = PVR_CMD_VERTEX;
				mat_trans_nodiv_nomod(line.point1.x, line.point1.y, line.point1.z, vtx[0].x, vtx[0].y, ignored, vtx[0].z);
				vtx[0].x /= vtx[0].z;
				vtx[0].y /= vtx[0].z;
				vtx[0].z = 1/vtx[0].z;
				vtx[0].a = 0.5f;
				vtx[0].r = 1;
				vtx[0].g = 1;
				vtx[0].b = 1;

				vtx[1].flags = PVR_CMD_VERTEX;
				mat_trans_nodiv_nomod(line.point2.x, line.point2.y, line.point2.z, vtx[1].x, vtx[1].y, ignored, vtx[1].z);
				vtx[1].x /= vtx[1].z;
				vtx[1].y /= vtx[1].z;
				vtx[1].z = 1/vtx[1].z;
				vtx[1].a = 0.5f;
				vtx[1].r = 1;
				vtx[1].g = 1;
				vtx[1].b = 1;

				// Extend to generate a triangle strip
				vtx[2].flags = PVR_CMD_VERTEX;
				vtx[2].x = vtx[0].x + 3.f; // Offset for quad
				vtx[2].y = vtx[0].y + 3.f;
				vtx[2].z = vtx[0].z;
				vtx[2].a = 0.5f;
				vtx[2].r = 1;
				vtx[2].g = 1;
				vtx[2].b = 1;

				vtx[3].flags = PVR_CMD_VERTEX_EOL; // End of triangle strip
				vtx[3].x = vtx[1].x + 3.f; // Offset for quad
				vtx[3].y = vtx[1].y + 3.f;
				vtx[3].z = vtx[1].z;
				vtx[3].a = 0.5f;
				vtx[3].r = 1;
				vtx[3].g = 1;
				vtx[3].b = 1;

				if (vtx[0].z < 0 || vtx[1].z < 0) {
					continue;
				}
				pvr_prim(vtx, sizeof(pvr_vertex32_ut));
				pvr_prim(vtx + 1, sizeof(pvr_vertex32_ut));
				pvr_prim(vtx + 2, sizeof(pvr_vertex32_ut));
				pvr_prim(vtx + 3, sizeof(pvr_vertex32_ut));
			}

			for (int triangleNum = 0; triangleNum < debugRenderer.getNbTriangles(); triangleNum++) {
				auto triangles = debugRenderer.getTrianglesArray();
				auto triangle = triangles[triangleNum];
				float ignored;
				pvr_vertex32_ut vtx[3];

				mat_trans_nodiv_nomod(triangle.point1.x, triangle.point1.y, triangle.point1.z, vtx[0].x, vtx[0].y, ignored, vtx[0].z);
				vtx[0].x /= vtx[0].z;
				vtx[0].y /= vtx[0].z;
				vtx[0].z = 1/vtx[0].z + 1;
				vtx[0].a = 0.1f;
				vtx[0].r = 1;
				vtx[0].g = 1;
				vtx[0].b = 1;

				mat_trans_nodiv_nomod(triangle.point2.x, triangle.point2.y, triangle.point2.z, vtx[1].x, vtx[1].y, ignored, vtx[1].z);
				vtx[1].x /= vtx[1].z;
				vtx[1].y /= vtx[1].z;
				vtx[1].z = 1/vtx[1].z + 1;
				vtx[1].a = 0.1f;
				vtx[1].r = 1;
				vtx[1].g = 1;
				vtx[1].b = 1;

				mat_trans_nodiv_nomod(triangle.point3.x, triangle.point3.y, triangle.point3.z, vtx[2].x, vtx[2].y, ignored, vtx[2].z);
				vtx[2].x /= vtx[2].z;
				vtx[2].y /= vtx[2].z;
				vtx[2].z = 1/vtx[2].z + 1;
				vtx[2].a = 0.1f;
				vtx[2].r = 1;
				vtx[2].g = 1;
				vtx[2].b = 1;
				vtx[0].flags = PVR_CMD_VERTEX;
				vtx[1].flags = PVR_CMD_VERTEX;
				vtx[2].flags = PVR_CMD_VERTEX_EOL;

				if (vtx[0].z < 0 || vtx[1].z < 0 || vtx[2].z < 0) {
					continue;
				}

				pvr_prim(vtx, sizeof(pvr_vertex32_ut));
				pvr_prim(vtx + 1, sizeof(pvr_vertex32_ut));
				pvr_prim(vtx + 2, sizeof(pvr_vertex32_ut));
			}
		}
		#endif
        if (vertexOverflown()) {
			freeVertexTarget += freeVertexTarget_Step_Up;
		} else {
			freeVertexTarget -= freeVertexTarget_Step_Down;
			if (freeVertexTarget < freeVertexTarget_Min) {
				freeVertexTarget = freeVertexTarget_Min;
			}
		}
        pvr_list_finish();

        leave_oix();
        pvr_scene_finish();
    }

    pvr_shutdown();
}