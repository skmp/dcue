#include <dc/pvr.h>
#include <dc/maple.h>
#include <dc/matrix.h>
#include <dc/maple/controller.h>
#include "dcue/types-native.h"
#include "vendor/gldc/alloc.h"

#include "vendor/dca3/float16.h"

#include <vector>
#include <fstream>
#include <iostream>

#define MAX_LIGHTS 8

#define dcache_pref_block(x) do { } while (false)
#define frsqrt(a) 				(1.0f/sqrt(a))
#if defined(DC_SH4)
#define FLUSH_TA_DATA(src) do { __asm__ __volatile__("ocbwb @%0" : : "r" (src) : "memory"); } while(0)
#else
#define FLUSH_TA_DATA(src) do { pvr_dr_commit(src); } while(0)
#endif

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
std::vector<mesh_t*> meshes;
std::vector<game_object_t*> gameObjects;

bool loadScene(const char* scene) {
    std::ifstream in(scene, std::ios::binary);
    if (!in) {
        std::cout << "Failed to open file: " << scene << std::endl;
        return false;
    }

    // Read and verify header (8 bytes)
    char header[9] = { 0};
    in.read(header, 8);
    if (strncmp(header, "DCUENS00", 8) != 0) {
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
        in.read((char*)&go->active, sizeof(go->active));
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
        if (tmp != UINT32_MAX) {
            assert(tmp < materials.size());
            go->material = materials[tmp];
        } else {
            go->material = nullptr;
        }
    }

    in.close();
    printf("Loaded %d textures, %d materials, %d meshes, and %d game objects.\n",
           static_cast<int>(textures.size()), static_cast<int>(materials.size()),
           static_cast<int>(meshes.size()), static_cast<int>(gameObjects.size()));

    return true;
}


matrix_t DCE_MAT_SCREENVIEW = {
    { 1, 0, 0, 0},
    { 0, 1, 0, 0},
    { 0, 0, 1, 0},
    { 0, 0, 0, 1}
};

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

void DCE_MatrixViewport(float x, float y, float width, float height) {
    DCE_MAT_SCREENVIEW[0][0] = -width * 0.5f;
    DCE_MAT_SCREENVIEW[1][1] = height * 0.5f;
    DCE_MAT_SCREENVIEW[2][2] = 1;
    DCE_MAT_SCREENVIEW[3][0] = -DCE_MAT_SCREENVIEW[0][0] + x;
    DCE_MAT_SCREENVIEW[3][1] = height - (DCE_MAT_SCREENVIEW[1][1] + y); 
}

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

RGBAf ambLight = { 0.4f, 0.4f, 0.4f, 1.0f };
float ambient = 1.0f;
RGBAf diffuseLight = { 0.8f, 0.8f, 0.8f, 1.0f };
float diffuse = 1.0f;


void invertGeneral(r_matrix_t *dst, const r_matrix_t *src)
{
	float det, invdet;
	// calculate a few cofactors
	dst->right.x = src->up.y*src->at.z - src->up.z*src->at.y;
	dst->right.y = src->at.y*src->right.z - src->at.z*src->right.y;
	dst->right.z = src->right.y*src->up.z - src->right.z*src->up.y;
	// get the determinant from that
	det = src->up.x * dst->right.y + src->at.x * dst->right.z + dst->right.x * src->right.x;
	invdet = 1.0;
	if(det != 0.0f)
		invdet = 1.0f/det;
	dst->right.x *= invdet;
	dst->right.y *= invdet;
	dst->right.z *= invdet;
	dst->up.x = invdet * (src->up.z*src->at.x - src->up.x*src->at.z);
	dst->up.y = invdet * (src->at.z*src->right.x - src->at.x*src->right.z);
	dst->up.z = invdet * (src->right.z*src->up.x - src->right.x*src->up.z);
	dst->at.x = invdet * (src->up.x*src->at.y - src->up.y*src->at.x);
	dst->at.y = invdet * (src->at.x*src->right.y - src->at.y*src->right.x);
	dst->at.z = invdet * (src->right.x*src->up.y - src->right.y*src->up.x);
	dst->pos.x = -(src->pos.x*dst->right.x + src->pos.y*dst->up.x + src->pos.z*dst->at.x);
	dst->pos.y = -(src->pos.x*dst->right.y + src->pos.y*dst->up.y + src->pos.z*dst->at.y);
	dst->pos.z = -(src->pos.x*dst->right.z + src->pos.y*dst->up.z + src->pos.z*dst->at.z);

    dst->at_w = 0;
    dst->up_w = 0;
    dst->right_w = 0;
    dst->pos_w = 1;
}

struct Camera {
    matrix_t devViewProjScreen;
    Plane frustumPlanes[6];

    game_object_t* go;

    float nearPlane;
    float farPlane;
    V2d viewOffset;
    V2d viewWindow;
    V3d frustumCorners[8];


    void buildPlanes()
    {
        V3d at = go->ltw.at;

        V3d *c = frustumCorners;
        Plane *p = frustumPlanes;
        V3d v51 = sub(c[1], c[5]);
        V3d v73 = sub(c[3], c[7]);

        /* Far plane */
        p[0].normal = at;
        p[0].distance = dot(p[0].normal, c[4]);

        /* Near plane */
        p[1].normal = neg(p[0].normal);
        p[1].distance = dot(p[1].normal, c[0]);

        /* Right plane */
        p[2].normal = normalize(cross(v51,
                                            sub(c[6], c[5])));
        p[2].distance = dot(p[2].normal, c[1]);

        /* Top plane */
        p[3].normal = normalize(cross(sub(c[4], c[5]),
                                            v51));
        p[3].distance = dot(p[3].normal, c[1]);

        /* Left plane */
        p[4].normal = normalize(cross(v73,
                                            sub(c[4], c[7])));
        p[4].distance = dot(p[4].normal, c[3]);

        /* Bottom plane */
        p[5].normal = normalize(cross(sub(c[6], c[7]),
                                            v73));
        p[5].distance = dot(p[5].normal, c[3]);
    }

    void buildClipPersp()
    {
        V3d up = go->ltw.up;
        V3d right = go->ltw.right;
        V3d at = go->ltw.at;
        V3d pos = go->ltw.pos;

        /* First we calculate the 4 points on the view window. */
        up = scale(up, viewWindow.y);
        V3d left = scale(right, viewWindow.x);
        V3d *c = frustumCorners;
        c[0] = add(add(at, up), left);	// top left
        c[1] = sub(add(at, up), left);	// top right
        c[2] = sub(sub(at, up), left);	// bottom right
        c[3] = add(sub(at, up), left);	// bottom left

        /* Now Calculate near and far corners. */
        V3d off = sub(scale(up, viewOffset.y), scale(right, viewOffset.x));
        for(int32 i = 0; i < 4; i++){
            V3d corner = sub(frustumCorners[i], off);
            V3d pos = add(pos, off);
            c[i] = add(scale(corner, nearPlane), pos);
            c[i+4] = add(scale(corner, farPlane), pos);
        }

        buildPlanes();
    }

    void setFOV(float hfov, float ratio)
    {
        V2d v;
        float w, h;

        w = (float)640;
        h = (float)480;
        if(w < 1 || h < 1){
            w = 1;
            h = 1;
        }
        hfov = hfov*3.14159f/360.0f;	// deg to rad and halved

        float ar1 = 4.0f/3.0f;
        float ar2 = w/h;
        float vfov = atanf(tanf(hfov/2) / ar1) *2;
        hfov = atanf(tanf(vfov/2) * ar2) *2;

        float a = tanf(hfov);
        viewWindow = { a, a/ratio };
        viewOffset = { 0.0f, 0.0f };
    }


    void beforeRender() {
        buildClipPersp();
        
        // calculate devViewProjScreen
        float view[16], proj[16];

        // View Matrix
        r_matrix_t inv;
        invertGeneral(&inv, &go->ltw);
        // Since we're looking into positive Z,
        // flip X to ge a left handed view space.
        view[0]  = inv.right.x;
        view[1]  =  -inv.right.y;
        view[2]  =  inv.right.z;
        view[3]  =  0.0f;
        view[4]  = inv.up.x;
        view[5]  =  -inv.up.y;
        view[6]  =  inv.up.z;
        view[7]  =  0.0f;
        view[8]  =  inv.at.x;
        view[9]  =   -inv.at.y;
        view[10] =  inv.at.z;
        view[11] =  0.0f;
        view[12] = inv.pos.x;
        view[13] =  -inv.pos.y;
        view[14] =  inv.pos.z;
        view[15] =  1.0f;
    
        // Projection Matrix
        float invwx = 1.0f/viewWindow.x;
        float invwy = 1.0f/viewWindow.y;
        float invz = 1.0f/(farPlane-nearPlane);
    
        proj[0] = invwx;
        proj[1] = 0.0f;
        proj[2] = 0.0f;
        proj[3] = 0.0f;
    
        proj[4] = 0.0f;
        proj[5] = invwy;
        proj[6] = 0.0f;
        proj[7] = 0.0f;
    
        proj[8] = viewOffset.x*invwx;
        proj[9] = viewOffset.y*invwy;
        proj[12] = -proj[8];
        proj[13] = -proj[9];
        if(true /*projection == Camera::PERSPECTIVE*/){
            proj[10] = farPlane*invz;
            proj[11] = 1.0f;
    
            proj[15] = 0.0f;
        }else{
            proj[10] = invz;
            proj[11] = 0.0f;
    
            proj[15] = 1.0f;
        }
        proj[14] = -nearPlane*proj[10];
        
        DCE_MatrixViewport(0, 0, 640, 480);
        
        mat_load((matrix_t*)&DCE_MAT_SCREENVIEW);
        mat_apply((matrix_t*)&proj[0]);
        mat_apply((matrix_t*)&view[0]);
        mat_store((matrix_t*)&devViewProjScreen);
    }
    
    enum { SPHEREOUTSIDE, SPHEREBOUNDARY, SPHEREINSIDE, SPHEREBOUNDARY_NEAR /* frustumTestSphereEx only */};

    int frustumTestSphereNear(const Sphere *s) const
    {
        int res = SPHEREINSIDE;
        const Plane *p = this->frustumPlanes;
    
        // far
        float dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        // near
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        if(s->radius > -dist)
            res = SPHEREBOUNDARY_NEAR;
        p++;
    
        // others
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        dist = dot(p->normal, s->center) - p->distance;
        if(s->radius < dist)
            return SPHEREOUTSIDE;
        p++;
    
        return res;
    }
};

template<int list>
void renderMesh(Camera* cam, game_object_t* go) {
    pvr_poly_hdr_t hdr;
    bool textured = go->material->texture != nullptr;

    if (go->material->texture) {
        pvr_poly_cxt_txr_fast(
            &hdr,
            list,

            go->material->texture->flags,
            go->material->texture->lw,
            go->material->texture->lh,
            (uint8_t*)go->material->texture->data - go->material->texture->offs,

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
            PVR_CULLING_CW,
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
            PVR_CULLING_CW,
            PVR_FOG_DISABLE
        );
    }

    pvr_prim(&hdr, sizeof(hdr));

    RGBAf residual, material;
    // Ambient Alpha ALWAYS = 1.0
    residual.alpha = go->material->color.alpha;
    residual.red = ambLight.red * ambient * go->material->color.red;
    residual.green = ambLight.green * ambient * go->material->color.green;
    residual.blue = ambLight.blue * ambient * go->material->color.blue;
    material.alpha = go->material->color.alpha;
    material.red = (1.0f / 255.0f) * go->material->color.red;
    material.green = (1.0f / 255.0f) * go->material->color.green;
    material.blue = (1.0f / 255.0f) * go->material->color.blue;

    const MeshInfo* meshInfo = (const MeshInfo*)&go->mesh->data[0];
    auto meshletInfoBytes = &go->mesh->data[meshInfo->meshletOffset];

    RGBAf lightDiffuseColors[MAX_LIGHTS];

    auto cntDiffuse = 0;

    // lightDiffuseColors[0].red = material.red * diffuseLight.red * diffuse;
    // lightDiffuseColors[0].green = material.green * diffuseLight.green * diffuse;
    // lightDiffuseColors[0].blue = material.blue * diffuseLight.blue * diffuse;

    bool global_needsNoClip = false;

    auto global_visible = cam->frustumTestSphereNear(&go->mesh->bounding_sphere);
    if (global_visible == Camera::SPHEREOUTSIDE) {
        // printf("Outside frustum cull\n");
        return;
    } else if (global_visible == Camera::SPHEREINSIDE) {
        global_needsNoClip = true;
    }

    for (unsigned meshletNum = 0; meshletNum < meshInfo->meshletCount; meshletNum++) {
        auto meshlet = (const MeshletInfo*)meshletInfoBytes;
        meshletInfoBytes += sizeof(MeshletInfo) - 8 ; // (skin ? 0 : 8);

        unsigned clippingRequired = 0;

        if (!global_needsNoClip) {
            // if (!skin) {
            //     Sphere sphere = meshlet->boundingSphere;
            //     V3d::transformPoints(&sphere.center, &sphere.center, 1, atomic->getFrame()->getLTM());
                
            //     auto local_frustumTestResult = cam->frustumTestSphereNear(&sphere);;
            //     if ( local_frustumTestResult == Camera::SPHEREOUTSIDE) {
            //         // printf("Outside frustum cull\n");
            //         continue;
            //     }

            //     if (local_frustumTestResult == Camera::SPHEREBOUNDARY_NEAR) {
            //         clippingRequired = 1 + textured;
            //     }
            // } else {
                clippingRequired = 1 + textured;
            // }
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
            matrix_t ltw = {
                { go->ltw.m00, go->ltw.m10, go->ltw.m20, go->ltw.m30 },
                { go->ltw.m01, go->ltw.m11, go->ltw.m21, go->ltw.m31 },
                { go->ltw.m02, go->ltw.m12, go->ltw.m22, go->ltw.m32 },
                { go->ltw.m03, go->ltw.m13, go->ltw.m23, go->ltw.m33 }
            };

            mat_load(&cam->devViewProjScreen);
            mat_apply(&ltw);

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
#if 0
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
            auto normalPointer = &dcModel->data[meshlet->vertexOffset] + normalOffset;
            auto vtxSize = meshlet->vertexSize;
            if (skin) {
                vtxSize = 64;
                if (textured) {
                    normalPointer = OCR_SPACE + offsetof(pvr_vertex64_t, _dmy1);
                } else {
                    normalPointer = OCR_SPACE + offsetof(pvr_vertex64_t, a);
                }
            }
            tnlMeshletDiffuseColorSelector[normalSelector](OCR_SPACE + dstColOffset, normalPointer, meshlet->vertexCount, vtxSize, &lightDiffuseColors[0]);
        
            if (pass2) {
                unsigned normalSelector = (pass2 - 1);
                mat_load((matrix_t*)&uniformObject.dir[1][0][0]);
                tnlMeshletDiffuseColorSelector[normalSelector](OCR_SPACE + dstColOffset, normalPointer, meshlet->vertexCount, vtxSize, &lightDiffuseColors[4]);
            }
        }
#endif
        auto indexData = (int8_t*)&go->mesh->data[meshlet->indexOffset];

        if (!clippingRequired) {
            submitMeshletSelector[textured](OCR_SPACE, indexData, meshlet->indexCount);
        } else {
            clipAndsubmitMeshletSelector[textured](OCR_SPACE, indexData, meshlet->indexCount);
        }
    }
}

int main() {

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

    loadScene("dream.ndt");

    Camera cam;
    cam.go = new game_object_t();
    cam.go->ltw = r_matrix_t {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    cam.setFOV(45.0f, 4.0f / 3.0f);
    cam.nearPlane = 0.1f;
    cam.farPlane = 1000.0f;

    cam.buildClipPersp();

    for(;;) {
        // get input
        auto contMaple = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (contMaple) {
            auto state = (cont_state_t *)maple_dev_status(contMaple);

            if (state) {
                if (state->start) {
                    break;
                }

                mat_load((matrix_t*)&cam.go->ltw);

                float x_dir = 0;

                if (state->joyy > 64) {
                    x_dir = -1;
                } else if (state->joyy < -64) {
                    x_dir = 1;
                }

                matrix_t translation_matrix = {
                    { 1, 0, 0, 0 },
                    { 0, 1, 0, 0 },
                    { 0, 0, 1, 0 },
                    { x_dir, 0, 0, 1 },
                };

				float y_rot = 0;

				if (state->joyx > 64) {
					y_rot = 10.0f * (3.1415f / 180.0f); // Convert degrees to radians
				} else if (state->joyx < -64) {
					y_rot = -10.0f * (3.1415f / 180.0f); // Convert degrees to radians
				}

                matrix_t rotation_matrix = {
                    { cosf(y_rot), 0, -sinf(y_rot), 0 },
                    { 0, 1, 0, 0 },
                    { sinf(y_rot), 0, cosf(y_rot), 0 },
                    { 0, 0, 0, 1 },
                };

                mat_apply(&rotation_matrix);
                
                mat_apply(&translation_matrix);
                mat_store((matrix_t*)&cam.go->ltw);
            }
        }

        cam.beforeRender();

        // render frame
        pvr_set_zclip(0.0f);
        pvr_set_bg_color(0.5f, 0.5f, 0.5f);
		pvr_wait_ready();

        pvr_scene_begin();
        pvr_dr_init(&drState);
        pvr_list_begin(PVR_LIST_OP_POLY);
        for (auto& go: gameObjects) {
            if (go->active && go->mesh_enabled && go->mesh && go->material && go->material->color.alpha == 1) {
                renderMesh<PVR_LIST_OP_POLY>(&cam, go);
            }
        }
        pvr_list_finish();

        pvr_dr_init(&drState);
        pvr_list_begin(PVR_LIST_TR_POLY);
        for (auto& go: gameObjects) {
            if (go->active && go->mesh_enabled && go->mesh && go->material && go->material->color.alpha != 1) {
                renderMesh<PVR_LIST_TR_POLY>(&cam, go);
            }
        }
        pvr_list_finish();
        pvr_scene_finish();
    }

    pvr_shutdown();
}