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

#include "animations.h"
#include "hierarchy.h"
#include "components.h"
#include "scripts.h"
#include "cameras.h"
#include "physics.h"
#include "fonts.h"

// #define DEBUG_PHYSICS

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
    if (strncmp(header, "DCUENS01", 8) != 0) {
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
        in.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
        auto mesh = meshes[i] = (mesh_t*)malloc(sizeof(mesh_t) + tmp);

        in.read((char*)&mesh->bounding_sphere, sizeof(mesh->bounding_sphere));
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
            for (int materialNum = 0; materialNum < submesh_count; materialNum++) {
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
float ambient = 1.0f;
RGBAf diffuseLight = { 0.8f, 0.8f, 0.8f, 1.0f };
float diffuse = 0.5f;
r_matrix_t diffusePos { 
    1, 0, 0, 0,
    0, 0.7071068, -0.7071068, 0,
    0, 0.7071068, 0.7071068, 0,
    0, 3, 0, 1
};

size_t vertexBufferFree() {
    size_t end   = PVR_GET(PVR_TA_VERTBUF_END);
    size_t pos   = PVR_GET(PVR_TA_VERTBUF_POS);

    size_t free  = end - pos;

	return free;
}

bool vertexOverflown() {
	return  PVR_GET(PVR_TA_VERTBUF_POS) >= PVR_GET(PVR_TA_VERTBUF_END) ||
			PVR_GET(PVR_TA_OPB_POS)*4 >= PVR_GET(PVR_TA_OPB_END);
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
    float scaleRight = std::sqrt(mat.right.x * mat.right.x +
                                 mat.right.y * mat.right.y +
                                 mat.right.z * mat.right.z);

    float scaleUp = std::sqrt(mat.up.x * mat.up.x +
                              mat.up.y * mat.up.y +
                              mat.up.z * mat.up.z);

    float scaleAt = std::sqrt(mat.at.x * mat.at.x +
                              mat.at.y * mat.at.y +
                              mat.at.z * mat.at.z);

    // Return the maximum scale factor
    return std::max({scaleRight, scaleUp, scaleAt});
}

#if defined(DEBUG_LOOKAT)
game_object_t* pointedGameObject = nullptr;
#endif

template<int list>
void renderMesh(camera_t* cam, game_object_t* go) {
    if (vertexBufferFree() < freeVertexTarget) {
        return;
    }
    bool global_needsNoClip = false;

	mat_load((matrix_t*)&go->ltw);
	Sphere sphere = go->mesh->bounding_sphere;
	float w;
	mat_trans_nodiv_nomod(sphere.center.x, sphere.center.y, sphere.center.z, sphere.center.x, sphere.center.y, sphere.center.z, w);
    float maxScaleFactor = GetMaxScale(go->ltw);
    sphere.radius *= maxScaleFactor;
    auto global_visible = cam->frustumTestSphereNear(&sphere);
    if (global_visible == camera_t::SPHEREOUTSIDE) {
        // printf("Outside frustum cull (%f, %f, %f) %f\n", sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius);
        return;
    } else if (global_visible == camera_t::SPHEREINSIDE) {
        global_needsNoClip = true;
    } else {
        // printf("Needs local clip (%f, %f, %f) %f\n", sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius);
    }

    auto cntDiffuse = 1;
    r_matrix_t invLtw;
    invertGeneral(&invLtw, &go->ltw);

    UniformObject uniformObject;
    mat_load((matrix_t*)&invLtw);
    {
        unsigned n = 0;
        uniformObject.col[n] = diffuseLight;
        mat_trans_nodiv_nomod_zerow(
            diffusePos.at.x, diffusePos.at.y, diffusePos.at.z,
            uniformObject.dir[n>>2][0][n&3],
            uniformObject.dir[n>>2][1][n&3],
            uniformObject.dir[n>>2][2][n&3],
            uniformObject.dir[n>>2][3][n&3]
        );

        uniformObject.dir[n>>2][3][n&3] = 0;
    }

    const MeshInfo* meshInfo = (const MeshInfo*)&go->mesh->data[0];

    for (int submesh_num = 0; submesh_num < go->submesh_count; submesh_num++) {

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
                PVR_DEPTHCMP_GREATER,
                PVR_DEPTHWRITE_ENABLE,
                PVR_CULLING_SMALL,
                PVR_FOG_DISABLE
            );
        } else {
            pvr_poly_cxt_col_fast(
                &hdr,
                list,

                PVR_CLRFMT_4FLOATS,
                list != PVR_LIST_OP_POLY ? PVR_BLEND_SRCALPHA : PVR_BLEND_ONE,
                list != PVR_LIST_OP_POLY ? PVR_BLEND_INVSRCALPHA : PVR_BLEND_ZERO,
                PVR_DEPTHCMP_GREATER,
                PVR_DEPTHWRITE_ENABLE,
                PVR_CULLING_SMALL,
                PVR_FOG_DISABLE
            );
        }

        pvr_prim(&hdr, sizeof(hdr));

        RGBAf residual, material;
        // Ambient Alpha ALWAYS = 1.0
        residual.alpha = go->materials[submesh_num]->color.alpha;
        residual.red = ambLight.red * ambient * go->materials[submesh_num]->color.red;
        residual.green = ambLight.green * ambient * go->materials[submesh_num]->color.green;
        residual.blue = ambLight.blue * ambient * go->materials[submesh_num]->color.blue;
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

        for (unsigned i = 0; i < cntDiffuse; i++) {
            lightDiffuseColors[i].red = material.red * uniformObject.col[i].red * diffuse;
            lightDiffuseColors[i].green = material.green * uniformObject.col[i].green * diffuse;
            lightDiffuseColors[i].blue = material.blue * uniformObject.col[i].blue * diffuse;
        }

        auto meshletInfoBytes = &go->mesh->data[meshInfo[submesh_num].meshletOffset];
        
        for (unsigned meshletNum = 0; meshletNum < meshInfo[submesh_num].meshletCount; meshletNum++) {
            auto meshlet = (const MeshletInfo*)meshletInfoBytes;
            meshletInfoBytes += sizeof(MeshletInfo) - 8 ; // (skin ? 0 : 8);

            unsigned clippingRequired = 0;

            if (!global_needsNoClip) {
                Sphere sphere = meshlet->boundingSphere;
                mat_load((matrix_t*)&go->ltw);
                float w;
                mat_trans_nodiv_nomod(sphere.center.x, sphere.center.y, sphere.center.z, sphere.center.x, sphere.center.y, sphere.center.z, w);
                sphere.radius *= maxScaleFactor;
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

            auto indexData = (int8_t*)&go->mesh->data[meshlet->indexOffset];

            if (!clippingRequired) {
                submitMeshletSelector[textured](OCR_SPACE, indexData, meshlet->indexCount);
            } else {
                clipAndsubmitMeshletSelector[textured](OCR_SPACE, indexData, meshlet->indexCount);
            }
        }
    }
}

// extern "C" cus also used by KOS
extern "C" const char* getExecutableTag() {
	return "tlj "  ":" ;
}

const char* lookAtMessage = nullptr;
const char* messageSpeaker = nullptr;
const char* messageText = nullptr;

const char** lookAtAction = nullptr;
int lookAtActionIndex = -1;

void setGameObject(component_type_t type, component_base_t* component, native::game_object_t* gameObject) {
    if (type >= ct_interaction) {
        auto interaction = (interaction_t*)component;
        interaction->gameObject = gameObject;
    } else {
        component->gameObject = gameObject;
    }
}

void animator_t::update(float deltaTime) {
    currentTime += deltaTime;
    for (size_t i = 0; i < num_bound_animations; ++i) {
        auto& boundAnim = bound_animations[i];
        for (size_t j = 0; j < boundAnim.animation->num_tracks; ++j) {
            auto& track = boundAnim.animation->tracks[j];
            auto& binding = boundAnim.bindings[j];
            auto& currentFrame = boundAnim.currentFrames[j];
            if (currentFrame >= track.num_keys - 1) {
                continue;
            }
            while (currentFrame < track.num_keys - 1 && currentTime >= track.times[currentFrame + 1]) {
                ++currentFrame;
            }
            if (currentFrame >= track.num_keys - 1) {
                currentFrame = track.num_keys - 1;
            }
            float t = (currentTime - track.times[currentFrame]) / (track.times[currentFrame + 1] - track.times[currentFrame]);
            auto& value = track.values[currentFrame];
            auto& nextValue = track.values[currentFrame + 1];
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

void native::game_object_t::setActive(bool active) {
	inactiveFlags = !active ? goi_inactive : 0;
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
	auto v = gameObjects[this->playaIndex]->position;
	auto y = gameObject->position;
	auto distance = sqrtf(v.x*y.x + v.y*y.y + v.z*y.z);

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
	// TODO
}

void show_message_t::interact() {
	if (messages) {
		nextMessage();
		if (timedHide) {
			timeToGo = time;
			if (timedHideCameraLock) {
				// TODO pavo push state
			}
		} else {
			// TODO pavo push state
		}
	}
}

// todo: onInteraction (pavo)

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
					// TODO pavo pop state
				}
			}
		}
	}
}

// TODO: manage well known objects in the scene better
game_object_t* playa;

void player_movement_t::update(float deltaTime) {
	playa = gameObject;

	if (!canMove) {
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

	movement *= speed * deltaTime;

	
	float movementX = sin(gameObject->rotation.y * deg2rad) * movement;
	float movementZ = cos(gameObject->rotation.y * deg2rad) * movement;
	gameObject->position.x += movementX;
	gameObject->position.z += movementZ;
}

class RaycastDumper: public reactphysics3d::RaycastCallback {
	box_collider_t* collider = nullptr;
public:
	virtual float notifyRaycastHit(const reactphysics3d::RaycastInfo& raycastInfo) override {
		auto collider2 = (box_collider_t*)raycastInfo.collider->getUserData();
		if (collider2->gameObject->isActive()) {
			collider = collider2;
			return raycastInfo.hitFraction;
		} else {
			return -1;
		}
	}

	void showMessage() {
		lookAtMessage = nullptr;
		const char** newLookAtAction = nullptr;
		if (collider) {
			#if defined(DEBUG_LOOKAT)
			pointedGameObject = collider->gameObject;
			#endif
			// std::cout << "Hit collider: " << collider << " gameObject " << collider->gameObject << std::endl;

			if (auto component = collider->gameObject->getComponents<interactable_t>()) {
				do {
					lookAtMessage = (*component)->lookAtMessage;
					if ((*component)->messages) {
						newLookAtAction = (*component)->messages;
					}
					break;
				} while (*++component);
			}
		}

		if (newLookAtAction != lookAtAction) {
			lookAtAction = newLookAtAction;
			lookAtActionIndex = -1;
		}
	}
};

void mouse_look_t::update(float deltaTime) {
	auto contMaple = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	if (!contMaple) {
		return;
	}
	auto state = (cont_state_t *)maple_dev_status(contMaple);
	if (!state) {
		return;
	}

	if (lookEnabled) {
		if (rotateEnabled) {
			gameObjects[playerBodyIndex]->rotation.y += (float)state->joyx * rotateSpeed * deltaTime;
			gameObject->rotation.x += (float)state->joyy * rotateSpeed * deltaTime;
			gameObject->rotation.x = std::clamp(gameObject->rotation.x, -89.0f, 89.0f);
		}
	}

	reactphysics3d::Vector3 cameraPos = {gameObject->ltw.pos.x, gameObject->ltw.pos.y, gameObject->ltw.pos.z};
	reactphysics3d::Vector3 cameraAt = {gameObject->ltw.at.x, gameObject->ltw.at.y, gameObject->ltw.at.z};
	reactphysics3d::Ray ray(cameraPos, cameraPos + cameraAt*100);

	#if defined(DEBUG_LOOKAT)
	pointedGameObject = nullptr;
	#endif
	RaycastDumper dumper;

	// physics is one step behind here
	physicsWorld->raycast(ray, &dumper);

	dumper.showMessage();
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
		(playa->position.x - gameObject->position.x) * (playa->position.x - gameObject->position.x) +
		(playa->position.y - gameObject->position.y) * (playa->position.y - gameObject->position.y) +
		(playa->position.z - gameObject->position.z) * (playa->position.z - gameObject->position.z)
	);

	if (distance < radius) {
		if (requiresItem) {
			// TODO: pavo invetory check
		}
		// TODO: fluctuate FOV
		// TODO: Fade
		teleport();
	}
}

void teleporter_t::teleport() {
	if (setPosition) {
		playa->position = gameObjects[destinationIndex]->position;
	}
	if (setRotation) {
		playa->rotation = gameObjects[destinationIndex]->rotation;
	}
}

V3d ComputeAxisAlignedScale(const r_matrix_t* mtx) {
    V3d scale;
	scale.x = sqrt(mtx->right.x * mtx->right.x + mtx->right.y * mtx->right.y + mtx->right.z * mtx->right.z);
	scale.y = sqrt(mtx->up.x    * mtx->up.x    + mtx->up.y    * mtx->up.y    + mtx->up.z    * mtx->up.z);
	scale.z = sqrt(mtx->at.x    * mtx->at.x    + mtx->at.y    * mtx->at.y    + mtx->at.z    * mtx->at.z);
    return scale;
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

	reactphysics3d::Transform t;
	t.setFromOpenGL(&gameObject->ltw.m00);
	rigidBody->setTransform(t);

	V3d scale = ComputeAxisAlignedScale(&gameObject->ltw);
	//V3d scale = { std::abs(gameObject->scale.x), std::abs(gameObject->scale.y), std::abs(gameObject->scale.z)};
	if (lastScale != scale || boxShape == nullptr) {
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroyBoxShape(boxShape);
		}
		boxShape = physicsCommon.createBoxShape(reactphysics3d::Vector3(scale.x * halfSize.x, scale.y * halfSize.y, scale.z * halfSize.z));
		collider = rigidBody->addCollider(boxShape, reactphysics3d::Transform(reactphysics3d::Vector3(scale.x * center.x, scale.x * center.y, scale.x *center.z), reactphysics3d::Quaternion::identity()));
		collider->setUserData(this);
		lastScale = scale;
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

	
	reactphysics3d::Transform t;
	t.setFromOpenGL(&gameObject->ltw.m00);
	rigidBody->setTransform(t);

	V3d scale3 = ComputeAxisAlignedScale(&gameObject->ltw);
	float scale = std::max(scale3.x, std::max(scale3.y, scale3.z));
	if (lastScale != scale3 || sphereShape == nullptr) {
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroySphereShape(sphereShape);
		}
		sphereShape = physicsCommon.createSphereShape(scale * radius);
		collider = rigidBody->addCollider(sphereShape, reactphysics3d::Transform(reactphysics3d::Vector3(center.x * scale3.x, center.y* scale3.y, center.z * scale3.z), reactphysics3d::Quaternion::identity()));
		collider->setUserData(this);
		lastScale = scale3;
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

	reactphysics3d::Transform t;
	t.setFromOpenGL(&gameObject->ltw.m00);
	rigidBody->setTransform(t);

	V3d scale3 = ComputeAxisAlignedScale(&gameObject->ltw);
	V2d scale = V2d(std::max(scale3.x, scale3.z), scale3.y);

	if (lastScale != scale3 || capsuleShape == nullptr) {
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroyCapsuleShape(capsuleShape);
		}
		capsuleShape = physicsCommon.createCapsuleShape(scale.x * radius, scale.y * height);
		collider = rigidBody->addCollider(capsuleShape, reactphysics3d::Transform(reactphysics3d::Vector3(center.x * scale3.x, center.y * scale3.y, center.z * scale3.z), reactphysics3d::Quaternion::identity()));
		collider->setUserData(this);
		lastScale = scale3;
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

	V3d scale = ComputeAxisAlignedScale(&gameObject->ltw);
	
	if (lastScale != scale || meshShape == nullptr) {
		if (indexCount == 0 || vertexCount == 0) {
			return;
		}
		if (collider) {
			rigidBody->removeCollider(collider);
			physicsCommon.destroyConcaveMeshShape(meshShape);
		}
		if (!triangleMesh) {
			triangleMesh = mesh_collider_triangles[{vertices, indices}];
			if (triangleMesh == nullptr) {
				reactphysics3d::TriangleVertexArray triangleVertexArray(
					vertexCount, vertices, sizeof(float)*3, indexCount/3, indices, sizeof(int16_t)*3,
					reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
					reactphysics3d::TriangleVertexArray::IndexDataType::INDEX_SHORT_TYPE
				);
				std::vector<reactphysics3d::Message> messages;
				triangleMesh = physicsCommon.createTriangleMesh(triangleVertexArray, messages);
				if (triangleMesh == nullptr) {
					// Handle error
					std::cerr << "Failed to create triangle mesh shape" << std::endl;

					for (const auto& message : messages) {
						std::cerr << "Message: " << message.text << std::endl;
					}
					assert(false && "Failed to create triangle mesh shape");
				}

				mesh_collider_triangles[{vertices, indices}] = triangleMesh;
			}
		}
		meshShape = physicsCommon.createConcaveMeshShape(triangleMesh, reactphysics3d::Vector3(scale.x, scale.y, scale.z));
		collider = rigidBody->addCollider(meshShape, reactphysics3d::Transform::identity());
		collider->setUserData(this);
		lastScale = scale;
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

	#if defined(DEBUG_PHYSICS)
	physicsWorld->setIsDebugRenderingEnabled(true);
	reactphysics3d::DebugRenderer& debugRenderer = physicsWorld->getDebugRenderer();
	debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLIDER_AABB, true);
	//debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
	#endif

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

				static bool last_b = false;
				if (!last_b && state->b && lookAtAction) {
					lookAtActionIndex++;
					if (!lookAtAction || lookAtAction[lookAtActionIndex] == nullptr) {
						lookAtActionIndex = -1;
						messageSpeaker = nullptr;
						messageText = nullptr;
					} else {
						messageText = lookAtAction[lookAtActionIndex];
					}
				}
				last_b = state->b;
            }
        }
        
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
		for (auto player_movement = player_movements; *player_movement; player_movement++) {
			if ((*player_movement)->gameObject->isActive()) {
				(*player_movement)->update(deltaTime);
			}
		}
		for (auto mouse_look = mouse_looks; *mouse_look; mouse_look++) {
			if ((*mouse_look)->gameObject->isActive()) {
				(*mouse_look)->update(deltaTime);
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

        for (auto go: gameObjects) {
			if (!go->isActive()) {
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
            } else {
                mat_load((matrix_t*)&identity_mtx);
            }

            mat_apply((matrix_t*)&pos_mtx);
            mat_apply((matrix_t*)&rot_mtx_y);
            mat_apply((matrix_t*)&rot_mtx_x);
            mat_apply((matrix_t*)&rot_mtx_z);
            mat_apply((matrix_t*)&scale_mtx);
            mat_store((matrix_t*)&go->ltw);
        }

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

        currentCamera->beforeRender(4.0f / 3.0f);

        // render frame
        pvr_set_zclip(0.0f);
        pvr_set_bg_color(0.5f, 0.5f, 0.5f);
		pvr_wait_ready();

        enter_oix();

        pvr_scene_begin();
        pvr_dr_init(&drState);
        pvr_list_begin(PVR_LIST_OP_POLY);

        for (auto& go: gameObjects) {
            if (go->isActive() && go->mesh_enabled && go->mesh && go->materials && go->materials[0]->color.alpha == 1) {
                renderMesh<PVR_LIST_OP_POLY>(currentCamera, go);
            }
        }
        pvr_list_finish();

        pvr_dr_init(&drState);
        pvr_list_begin(PVR_LIST_TR_POLY);
        for (auto& go: gameObjects) {
			if (go->isActive() && go->mesh_enabled && go->mesh && go->materials && go->materials[0]->color.alpha != 1) {
				renderMesh<PVR_LIST_TR_POLY>(currentCamera, go);
            }
        }

		// drawTextCentered(&fonts_19, 24, 320, 240, "Hello M World", 1, 1, 0.1, 0.1);
		// drawTextCentered(&fonts_0, 11, 320, 240 + 100, "Hello M World", 1, 1, 0.1, 0.1);

		if (messageText) {
			if (messageSpeaker) {
				drawTextLeftBottom(&fonts_1, 20, 40, 390, messageSpeaker, 1, 1, 1, 1);
			}
			drawTextLeftBottom(&fonts_0, 15, 40, 440, messageText, 1, 1, 1, 1);
		} else if (lookAtMessage) {
			drawTextCentered(&fonts_19, 24, 320, 240, lookAtMessage, 1, 1, 0.1, 0.1);
		}
		#if defined(DEBUG_PHYSICS)
		pvr_poly_hdr_t hdr;
		pvr_poly_cxt_col_fast(
			&hdr,
			PVR_LIST_TR_POLY,
			PVR_CLRFMT_4FLOATS,
			PVR_BLEND_SRCALPHA,
			PVR_BLEND_INVSRCALPHA,
			PVR_DEPTHCMP_GREATER,
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