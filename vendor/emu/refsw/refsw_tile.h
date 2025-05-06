#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "pvr_regs.h"
#include "pvr_mem.h"
#include "core_structs.h"



#include "refsw_lists.h"

// For texture cache

#define MAX_RENDER_WIDTH 32
#define MAX_RENDER_HEIGHT 32
#define MAX_RENDER_PIXELS (MAX_RENDER_WIDTH * MAX_RENDER_HEIGHT)

#define STRIDE_PIXEL_OFFSET MAX_RENDER_WIDTH


typedef float    ZType;
typedef u8       StencilType;
typedef u32      ColorType;
/*
    Surface equation solver
*/
struct PlaneStepper3
{
    float ddx, ddy;
    float c;

    void Setup(taRECT *rect, const Vertex& v1, const Vertex& v2, const Vertex& v3, float v1_a, float v2_a, float v3_a)
    {
        float Aa = ((v3_a - v1_a) * (v2.y - v1.y) - (v2_a - v1_a) * (v3.y - v1.y));
        float Ba = ((v3.x - v1.x) * (v2_a - v1_a) - (v2.x - v1.x) * (v3_a - v1_a));

        float C = ((v2.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (v2.y - v1.y));
        
        if (C == 0) {
            C = 1; // avoid divide by zero
        }

        ddx = -Aa / C;
        ddy = -Ba / C;

        c = v1_a - ddx * (v1.x - rect->left) - ddy * (v1.y - rect->top);
    }

    float Ip(float x, float y) const
    {
        return x * ddx + y * ddy + c;
    }

    float Ip(float x, float y, float W) const
    {
        return Ip(x, y) * W;
    }

    float IpU8(float x, float y, float W) const
    {
        float rv = Ip(x, y, W);

        if (rv < 0) rv = 0;
        if (rv > 255) rv = 255;

        return rv;
    }
};

/*
    Interpolation helper
*/
struct IPs3
{
    PlaneStepper3 invW;
    PlaneStepper3 U[2];
    PlaneStepper3 V[2];
    PlaneStepper3 Col[2][4];
    PlaneStepper3 Ofs[2][4];

    void Setup(taRECT *rect, DrawParameters* params, const Vertex& v1, const Vertex& v2, const Vertex& v3, bool TwoVolumes)
    {
        invW.Setup(rect, v1, v2, v3, v1.z, v2.z, v3.z);
        U[0].Setup(rect, v1, v2, v3, v1.u * v1.z, v2.u * v2.z, v3.u * v3.z);
        V[0].Setup(rect, v1, v2, v3, v1.v * v1.z, v2.v * v2.z, v3.v * v3.z);
        if (params->isp.Gouraud) {
            for (int i = 0; i < 4; i++)
                Col[0][i].Setup(rect, v1, v2, v3, v1.col[i] * v1.z, v2.col[i] * v2.z, v3.col[i] * v3.z);

            for (int i = 0; i < 4; i++)
                Ofs[0][i].Setup(rect, v1, v2, v3, v1.spc[i] * v1.z, v2.spc[i] * v2.z, v3.spc[i] * v3.z);
        } else {
            for (int i = 0; i < 4; i++)
                Col[0][i].Setup(rect, v1, v2, v3, v3.col[i] * v1.z, v3.col[i] * v2.z, v3.col[i] * v3.z);

            for (int i = 0; i < 4; i++)
                Ofs[0][i].Setup(rect, v1, v2, v3, v3.spc[i] * v1.z, v3.spc[i] * v2.z, v3.spc[i] * v3.z);
        }

        if (TwoVolumes) {
            U[1].Setup(rect, v1, v2, v3, v1.u1 * v1.z, v2.u1 * v2.z, v3.u1 * v3.z);
            V[1].Setup(rect, v1, v2, v3, v1.v1 * v1.z, v2.v1 * v2.z, v3.v1 * v3.z);
            if (params->isp.Gouraud) {
                for (int i = 0; i < 4; i++)
                    Col[1][i].Setup(rect, v1, v2, v3, v1.col1[i] * v1.z, v2.col1[i] * v2.z, v3.col1[i] * v3.z);

                for (int i = 0; i < 4; i++)
                    Ofs[1][i].Setup(rect, v1, v2, v3, v1.spc1[i] * v1.z, v2.spc1[i] * v2.z, v3.spc1[i] * v3.z);
            } else {
                for (int i = 0; i < 4; i++)
                    Col[1][i].Setup(rect, v1, v2, v3, v3.col1[i] * v1.z, v3.col1[i] * v2.z, v3.col1[i] * v3.z);

                for (int i = 0; i < 4; i++)
                    Ofs[1][i].Setup(rect, v1, v2, v3, v3.spc1[i] * v1.z, v3.spc1[i] * v2.z, v3.spc1[i] * v3.z);
            }
        }
    }
};

// Used for deferred TSP processing lookups
struct FpuEntry
{
    IPs3 ips;
    DrawParameters params;
};

union Color {
    u32 raw;
    u8 bgra[4];
    struct {
        u8 b;
        u8 g;
        u8 r;
        u8 a;
    };
};

extern u32  colorBuffer1 [MAX_RENDER_PIXELS];
extern const char* dump_textures;

void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue);
void ClearParamStatusBuffer();
void PeelBuffers(float depthValue, u32 stencilValue);
void PeelBuffersPT();
void PeelBuffersPTInitial(float depthValue);
void SummarizeStencilOr();
void SummarizeStencilAnd();
void ClearMoreToDraw();
bool GetMoreToDraw();

// Render to ACCUM from TAG buffer
// TAG holds references to triangles, ACCUM is the tile framebuffer
template<RenderMode rm>
void RenderParamTags(int tileX, int tileY);

inline __attribute__((always_inline)) f32 f16(u16 v)
{
    u32 z=v<<16;
    return *(f32*)&z;
}

//decode a vertex in the native pvr format
void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr,Vertex* cv, u32 shadow);
// decode an object (params + vertexes)
u32 decode_pvr_vertices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 two_volumes, Vertex* vtx, int count, int offset);

const FpuEntry& GetFpuEntry(taRECT *rect, RenderMode render_mode, ISP_BACKGND_T_type core_tag);
// Lookup/create cached TSP parameters, and call PixelFlush_tsp
bool PixelFlush_tsp(bool pp_AlphaTest, const FpuEntry* entry, float x, float y, u32 index, float invW, bool InVolume);
// Rasterize a single triangle to ISP (or ISP+TSP for PT)

extern void (*RasterizeTriangle_table[])(DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area);

u8* GetColorOutputBuffer();


/*
    Main renderer class
*/

void RenderTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area);
// called on vblank
bool RenderFramebuffer();
u32 ReadRegionArrayEntry(u32 base, RegionArrayEntry* entry);
ISP_BACKGND_T_type CoreTagFromDesc(u32 cache_bypass, u32 shadow, u32 skip, u32 param_offs_in_words, u32 tag_offset);
void RenderTriangleStrip(RenderMode render_mode, ObjectListEntry obj, taRECT* rect);
void RenderTriangleArray(RenderMode render_mode, ObjectListEntry obj, taRECT* rect);
void RenderQuadArray(RenderMode render_mode, ObjectListEntry obj, taRECT* rect);
void RenderObjectList(RenderMode render_mode, pvr32addr_t base, taRECT* rect);
void RenderCORE();
void Hackpresent();
void ClearFpuCache();