/*
	This file is part of libswirl

   Implementes the Reference SoftWare renderer (RefRendInterface) backend for refrend_base.
   This includes buffer operations and rasterization

   Pixel operations are in refsw_pixel.cpp (PixelPipeline interface)
*/
#include "license/bsd"

#include <cmath>
#include <memory>
#include <set>
#include <float.h>
#include <cstdio>
#include <cstdlib>

#include "refsw_tile.h"
#include "TexUtils.h"

parameter_tag_t tagBuffer[2] [MAX_RENDER_PIXELS];
StencilType     stencilBuffer[MAX_RENDER_PIXELS];
u32             colorBuffer1 [MAX_RENDER_PIXELS];
u32             colorBuffer2 [MAX_RENDER_PIXELS];
ZType           depthBuffer[2] [MAX_RENDER_PIXELS];

u32 tagBufferA;
u32 tagBufferB;
u32 depthBufferA;
u32 depthBufferB;

static float mmin(float a, float b, float c, float d)
{
    float rv = std::min(a, b);
    rv = std::min(c, rv);
    return std::max(d, rv);
}

static float mmax(float a, float b, float c, float d)
{
    float rv = std::max(a, b);
    rv = std::max(c, rv);
    return std::min(d, rv);
}


// Z buffer doesn't store sign, and has 19 bits of m
f32  mask_w(f32 w) {
    // u32 wu = (u32&)w;
    // wu = wu & 0x7FFFFFF8;
    // return (f32&)wu;
    return w;
}

void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue)
{
    depthBufferA = 0;
    depthBufferB = 1;

    tagBufferA = 0;
    tagBufferB = 1;

    auto zb = depthBuffer[depthBufferA];
    auto stencil = stencilBuffer;
    auto pb = tagBuffer[tagBufferA];

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        zb[i] = mask_w(depthValue);
        stencil[i] = stencilValue;
        pb[i] = paramValue;
    }
}

void ClearParamBuffer(parameter_tag_t paramValue) {
    tagBufferA = 0;
    tagBufferB = 1;

    auto pb = tagBuffer[tagBufferA];

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        pb[i] = paramValue;
    }
}

void PeelBuffersPTInitial(float depthValue) {
    tagBufferA = 1;
    tagBufferB = 0;

    auto zb2 = depthBuffer[depthBufferB];

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        zb2[i] = mask_w(depthValue);// set the "furthest" test to furthest value possible
        tagBuffer[tagBufferA][i] = TAG_INVALID;
    }
}

void PeelBuffersPT() {
    auto zb = depthBuffer[depthBufferA];
    auto zb2 = depthBuffer[depthBufferB];

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        zb2[i] = zb[i];  // keep old zb for 
    }
}

void PeelBuffersPTAfterHoles() {
    std::swap(tagBufferB, tagBufferA);
    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        tagBuffer[tagBufferA][i] = TAG_INVALID;
    }
}

void PeelBuffers(float depthValue, u32 stencilValue)
{
    std::swap(depthBufferB, depthBufferA);
    std::swap(tagBufferB, tagBufferA);

    auto zb = depthBuffer[depthBufferA];
    auto zb2 = depthBuffer[depthBufferB];
    auto stencil = stencilBuffer;

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        zb[i] = mask_w(depthValue);    // set the "closest" test to furthest value possible
        tagBuffer[tagBufferA][i] = TAG_INVALID;
        stencil[i] = stencilValue;
    }
}


void SummarizeStencilOr() {
    auto stencil = stencilBuffer;

    // post movdol merge INSIDE
    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        if (stencil[i] & 0b100) {
            stencil[i] |= (stencil[i] >>1);
            stencil[i] &=0b001; // keep only status bit
        }
    }
}

void SummarizeStencilAnd() {
    auto stencil = stencilBuffer;

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        // post movdol merge OUTSIDE
        if (stencil[i] & 0b100) {
            stencil[i] &= (stencil[i] >>1);
            stencil[i] &=0b001; // keep only status bit
        }
    }
}

bool MoreToDraw;
void ClearMoreToDraw()
{
    MoreToDraw = 0;
}

bool GetMoreToDraw()
{
    return MoreToDraw;
}

    // Render to ACCUM from TAG buffer
// TAG holds references to trianes, ACCUM is the tile framebuffer
void RenderParamTags(RenderMode rm, int tileX, int tileY) {
    float halfpixel = HALF_OFFSET.tsp_pixel_half_offset ? 0.5f : 0;
    taRECT rect;
    rect.left = tileX;
    rect.top = tileY;
    rect.bottom = rect.top + 32;
    rect.right = rect.left + 32;

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            auto index = y * 32 + x;
            auto tag =  tagBuffer[tagBufferA][index];
            ISP_BACKGND_T_type t { .full = tag & ~TAG_INVALID };
            bool InVolume = (stencilBuffer[index] & 0b001) == 0b001 && t.shadow;
            bool TagValid = !(tag & TAG_INVALID);
            
            if (TagValid || (rm == RM_OP_PT_MV && InVolume)) {
                auto Entry = GetFpuEntry(&rect, rm, t);
                auto invW = Entry.ips.invW.Ip(x + halfpixel, y + halfpixel);
                bool AlphaTestPassed = PixelFlush_tsp(rm == RM_PUNCHTHROUGH, &Entry, x + halfpixel, y + halfpixel, index, invW, InVolume);

                auto pb = tagBuffer[tagBufferA] + index;

                *pb |= TAG_INVALID;

                // can only happen when rm == RM_PUNCHTHROUGH
                if (!AlphaTestPassed) {
                     MoreToDraw = true;
                     // Feedback Channel
                     depthBuffer[depthBufferA][index] = ISP_BACKGND_D.f;
                }
            }
        }
    }
}

void ClearFpuEntries() {

}

f32 f16(u16 v)
{
    u32 z=v<<16;
    return *(f32*)&z;
}

#define vert_packed_color_(to,src) \
	{ \
	u32 t=src; \
	to[0] = (u8)(t);t>>=8;\
	to[1] = (u8)(t);t>>=8;\
	to[2] = (u8)(t);t>>=8;\
	to[3] = (u8)(t);      \
	}

//decode a vertex in the native pvr format
void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr, Vertex* cv, u32 two_volumes)
{
    //XYZ
    //UV
    //Base Col
    //Offset Col

    //XYZ are _allways_ there :)
    cv->x=vrf(ptr);ptr+=4;
    cv->y=vrf(ptr);ptr+=4;
    cv->z=vrf(ptr);ptr+=4;

    if (params->isp.Texture)
    {	//Do texture , if any
        if (params->isp.UV_16b)
        {
            u32 uv=vri(ptr);
            cv->u = f16((u16)(uv >>16));
            cv->v = f16((u16)(uv >> 0));
            ptr+=4;
        }
        else
        {
            cv->u=vrf(ptr);ptr+=4;
            cv->v=vrf(ptr);ptr+=4;
        }
    }

    //Color
    u32 col=vri(ptr);ptr+=4;
    vert_packed_color_(cv->col,col);
    if (params->isp.Offset)
    {
        //Intensity color
        u32 col=vri(ptr);ptr+=4;
        vert_packed_color_(cv->spc,col);
    }

    if (two_volumes) {
        if (params->isp.Texture)
        {	//Do texture , if any
            if (params->isp.UV_16b)
            {
                u32 uv=vri(ptr);
                cv->u1 = f16((u16)(uv >>16));
                cv->v1 = f16((u16)(uv >> 0));
                ptr+=4;
            }
            else
            {
                cv->u1=vrf(ptr);ptr+=4;
                cv->v1=vrf(ptr);ptr+=4;
            }
        }

        //Color
        u32 col1=vri(ptr);ptr+=4;
        vert_packed_color_(cv->col1,col1);
        if (params->isp.Offset)
        {
            //Intensity color
            u32 col1=vri(ptr);ptr+=4;
            vert_packed_color_(cv->spc1,col1);
        }
    }
}

// decode an object (params + vertexes)
u32 decode_pvr_vertices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 two_volumes, Vertex* vtx, int count, int offset)
{
    params->isp.full=vri(base);
    params->tsp[0].full=vri(base+4);
    params->tcw[0].full=vri(base+8);

    base += 12;
    if (two_volumes) {
        params->tsp[1].full=vri(base+0);
        params->tcw[1].full=vri(base+4);
        base += 8;
    }

    for (int i = 0; i < offset; i++) {
        base += (3 + skip * (two_volumes+1)) * 4;
    }

    for (int i = 0; i < count; i++) {
        decode_pvr_vertex(params,base, &vtx[i], two_volumes);
        base += (3 + skip * (two_volumes+1)) * 4;
    }

    return base;
}

FpuEntry GetFpuEntry(taRECT *rect, RenderMode render_mode, ISP_BACKGND_T_type core_tag)
{
    FpuEntry entry = {};
    Vertex vtx[3];
    decode_pvr_vertices(&entry.params, PARAM_BASE + core_tag.param_offs_in_words * 4, core_tag.skip, core_tag.shadow & ~FPU_SHAD_SCALE.intensity_shadow, vtx, 3, core_tag.tag_offset);

    entry.ips.Setup(rect, &entry.params, vtx[0], vtx[1], vtx[2], core_tag.shadow & ~FPU_SHAD_SCALE.intensity_shadow);

    return entry;
}

// Lookup/create cached TSP parameters, and call PixelFlush_tsp
bool PixelFlush_tsp(bool pp_AlphaTest, FpuEntry* entry, float x, float y, u32 index, float invW, bool InVolume)
{
    u32 two_voume_index = InVolume & !FPU_SHAD_SCALE.intensity_shadow;
    return PixelFlush_tsp(entry->params.tsp[two_voume_index].UseAlpha, entry->params.isp.Texture, entry->params.isp.Offset, entry->params.tsp[two_voume_index].ColorClamp, entry->params.tsp[two_voume_index].FogCtrl,
                                        entry->params.tsp[two_voume_index].IgnoreTexA, entry->params.tsp[two_voume_index].ClampU, entry->params.tsp[two_voume_index].ClampV, entry->params.tsp[two_voume_index].FlipU,  entry->params.tsp[two_voume_index].FlipV,  entry->params.tsp[two_voume_index].FilterMode,  entry->params.tsp[two_voume_index].ShadInstr,  
                                        pp_AlphaTest,  entry->params.tsp[two_voume_index].SrcSelect,  entry->params.tsp[two_voume_index].DstSelect,  entry->params.tsp[two_voume_index].SrcInstr,  entry->params.tsp[two_voume_index].DstInstr,
                                        entry, x, y, 1/invW, InVolume, index);
}

// this is disabled for now, as it breaks game scenes
bool IsTopLeft(float x, float y) {
    bool IsTop = y == 0 && x > 0;
    bool IsLeft = y < 0;

    // return IsTop || IsLeft;
    return true;
}

// Rasterize a single triangle to ISP (or ISP+TSP for PT)
void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area)
{
    const int stride_bytes = STRIDE_PIXEL_OFFSET * 4;
    //Plane equation

#define FLUSH_NAN(a) std::isnan(a) ? 0 : a

    const float Y1 = FLUSH_NAN(v1.y);
    const float Y2 = FLUSH_NAN(v2.y);
    const float Y3 = FLUSH_NAN(v3.y);
    const float Y4 = v4 ? FLUSH_NAN(v4->y) : 0;

    const float X1 = FLUSH_NAN(v1.x);
    const float X2 = FLUSH_NAN(v2.x);
    const float X3 = FLUSH_NAN(v3.x);
    const float X4 = v4 ? FLUSH_NAN(v4->x) : 0;

    int sgn = 1;

    float tri_area = ((X1 - X3) * (Y2 - Y3) - (Y1 - Y3) * (X2 - X3));

    if (tri_area > 0)
        sgn = -1;

    // cull
    if (params->isp.CullMode != 0) {
        //area: (X1-X3)*(Y2-Y3)-(Y1-Y3)*(X2-X3)

        float abs_area = fabsf(tri_area);

        if (abs_area < FPU_CULL_VAL)
            return;

        if (params->isp.CullMode >= 2) {
            u32 mode = params->isp.CullMode & 1;

            if (
                (mode == 0 && tri_area < 0) ||
                (mode == 1 && tri_area > 0)) {
                return;
            }
        }
    }

    // Bounding rectangle
//  int minx = mmin(X1, X2, X3, area->left);
//  int miny = mmin(Y1, Y2, Y3, area->top);

//  int spanx = mmax(X1+1, X2+1, X3+1, area->right - 1) - minx + 1;
//  int spany = mmax(Y1+1, Y2+1, Y3+1, area->bottom - 1) - miny + 1;

    //Inside scissor area?
//  if (spanx < 0 || spany < 0)
//      return;

    // Half-edge constants
    const float DX12 = sgn * (X1 - X2);
    const float DX23 = sgn * (X2 - X3);
    const float DX31 = v4 ? sgn * (X3 - X4) : sgn * (X3 - X1);
    const float DX41 = v4 ? sgn * (X4 - X1) : 0;

    const float DY12 = sgn * (Y1 - Y2);
    const float DY23 = sgn * (Y2 - Y3);
    const float DY31 = v4 ? sgn * (Y3 - Y4) : sgn * (Y3 - Y1);
    const float DY41 = v4 ? sgn * (Y4 - Y1) : 0;

    float C1 = DY12 * (X1 - area->left) - DX12 * (Y1 - area->top);
    float C2 = DY23 * (X2 - area->left) - DX23 * (Y2 - area->top);
    float C3 = DY31 * (X3 - area->left) - DX31 * (Y3 - area->top);
    float C4 = v4 ? DY41 * (X4 - area->left) - DX41 * (Y4 - area->top) : 1;

    C1 += IsTopLeft(DX12, DY12) ? 0 : -1;
    C2 += IsTopLeft(DX23, DY23) ? 0 : -1;
    C3 += IsTopLeft(DX31, DY31) ? 0 : -1;
    if (v4) {
        C4 += IsTopLeft(DX41, DY41) ? 0 : -1;
    }
    PlaneStepper3 Z;
    Z.Setup(area, v1, v2, v3, v1.z, v2.z, v3.z);

    float halfpixel = HALF_OFFSET.fpu_pixel_half_offset ? 0.5f : 0;
    float y_ps    = halfpixel;
    float minx_ps = halfpixel;

    // Loop through ALL pixels in the tile (no smart clipping)
	for (int y = 0; y < 32; y++)
    {
        float x_ps = minx_ps;
        for (int x = 0; x < 32; x++)
        {
            float Xhs12 = C1 + DX12 * y_ps - DY12 * x_ps;
            float Xhs23 = C2 + DX23 * y_ps - DY23 * x_ps;
            float Xhs31 = C3 + DX31 * y_ps - DY31 * x_ps;
            float Xhs41 = C4 + DX41 * y_ps - DY41 * x_ps;

            bool inTriangle = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;
			
            if (inTriangle) {
                u32 index = y * 32 + x;
                float invW = Z.Ip(x_ps, y_ps);
                PixelFlush_isp(render_mode, params->isp.DepthMode, params->isp.ZWriteDis, x_ps, y_ps, invW, index, tag);

                if (render_mode == RM_TRANSLUCENT && ISP_FEED_CFG.pre_sort && !(tagBuffer[tagBufferA][index] & TAG_INVALID)) {
                    ISP_BACKGND_T_type t { .full = tagBuffer[tagBufferA][index] };
                    auto Entry = GetFpuEntry(area, RM_TRANSLUCENT, t);
                    PixelFlush_tsp(false, &Entry, x_ps, y_ps, index, invW, false);
                    tagBuffer[tagBufferA][index] |= TAG_INVALID;
                }
            }

            x_ps = x_ps + 1;
        }
    next_y:
        y_ps = y_ps + 1;
    }
}
    
u8* GetColorOutputBuffer() {
    return (u8*)colorBuffer1;
}


// Clamp and flip a texture coordinate
static int ClampFlip( bool pp_Clamp, bool pp_Flip
	                , int coord, int size) {
    if (pp_Clamp) { // clamp
        if (coord < 0) {
            coord = 0;
        } else if (coord >= size) {
            coord = size-1;
        }
    } else if (pp_Flip) { // flip
        coord &= size*2-1;
        if (coord & size) {
            coord ^= size*2-1;
        }
    } else { //wrap
        coord &= size-1;
    }

    return coord;
}


const u32 MipPoint[11] =
{
    0x00003,//1
    0x00001 * 4,//2
    0x00002 * 4,//4
	0x00006 * 4,//8
	0x00016 * 4,//16
	0x00056 * 4,//32
	0x00156 * 4,//64
	0x00556 * 4,//128
	0x01556 * 4,//256
	0x05556 * 4,//512
	0x15556 * 4//1024
};

#if 0
static Color TextureFetchOld(TSP tsp, TCW tcw, int u, int v) {
    auto textel_stride = 8 << tsp.TexU;

    u32 start_address = tcw.TexAddr << 3;
    u32 base_address = start_address;
    
    u32 mip_bpp;
    if (tcw.VQ_Comp) {
        mip_bpp = 2;
    } else if (tcw.PixelFmt == PixelPal8) {
        mip_bpp = 8;
    }
    else if (tcw.PixelFmt == PixelPal4) {
        mip_bpp = 4;
    }
    else {
        mip_bpp = 16;
    }

    if (tcw.MipMapped) {
        base_address += MipPoint[tsp.TexU] * mip_bpp / 2;
    }

    u32 offset;
    if (tcw.VQ_Comp) {
        offset = twop(u, v, tsp.TexU, tsp.TexV) / 4;
    } else if (!tcw.ScanOrder) {
        offset = twop(u, v, tsp.TexU, tsp.TexV);
    } else {
        offset = u + textel_stride * v;
    }

    u16 memtel;
    if (tcw.VQ_Comp) {
        u8 index = emu_vram[(base_address + offset + 256*4*2) & VRAM_MASK];
        u16 *vq_book = (u16*)&emu_vram[start_address];
        memtel = vq_book[index * 4 + (u&1)*2 + (v&1) ];
    } else {
        memtel = (u16&)emu_vram[(base_address + offset *2) & VRAM_MASK];
    }

    u32 textel;
    switch (tcw.PixelFmt)
    {
        case PixelReserved:
        case Pixel1555: textel = ARGB1555_32(memtel); break;
        case Pixel565: textel = ARGB565_32(memtel); break;
        case Pixel4444: textel = ARGB4444_32(memtel); break;
        case PixelYUV: textel = 0xDE000EEF; break;
        case PixelBumpMap: textel = 0xDEADBEEF; break;
        case PixelPal4: textel = 0xFF00FFF0; break;
        case PixelPal8: textel = 0xFF0FF0FF; break;
    }
    return { .raw =  textel };
}
#endif

u32 ExpandToARGB8888(u32 color, u32 mode, bool ScanOrder /* TODO: Expansion Patterns */) {
	switch(mode)
	{
        case 0: return ARGB1555_32(color);
        case 1: return ARGB565_32(color);
        case 2: return ARGB4444_32(color);
        case 3: return ARGB8888_32(color);  // this one just shuffles
    }
    return 0xDEADBEEF;
}

u32 TexAddressGen(TCW tcw) {
    u32 base_address = tcw.TexAddr << 3;

    if (tcw.VQ_Comp) {
        base_address += 256 * 4 * 2;
    }

    return base_address;
}

u32 TexOffsetGen(TSP tsp, TCW tcw, bool ScanOrder, int u, int v, u32 stride, u32 MipLevel) {
    u32 mip_offset;
    
    if (tcw.MipMapped) {
        mip_offset = MipPoint[3 + tsp.TexU - MipLevel];
    } else {
        mip_offset = 0;
    }

    if (tcw.VQ_Comp || !ScanOrder) {
        if (tcw.MipMapped) {
            return mip_offset + twop(u, v, (tsp.TexU - MipLevel), (tsp.TexU - MipLevel));
        } else {
            return mip_offset + twop(u, v, tsp.TexU, tsp.TexV);
        }
    } else {
        return mip_offset + u + stride * v;
    }
}

// 4.1 format
u32 fBitsPerPixel(TCW tcw) {
    u32 rv;
    if (tcw.PixelFmt == PixelPal8) {
        rv = 8;
    }
    else if (tcw.PixelFmt == PixelPal4) {
        rv = 4;
    }
    else {
        rv = 16;
    }

    if (tcw.VQ_Comp) {
        return 8 * 2 / (64 / rv); // 8 bpp / (pixels per 64 bits)
    } else {
        return rv * 2;
    }
}

u64 VQLookup(u32 start_address, u64 memtel, u32 offset) {
    u8* memtel8 = (u8*)&memtel;

    u64 *vq_book = (u64*)&emu_vram[start_address & (VRAM_MASK-7)];
    u8 index = memtel8[offset & 7];
    return vq_book[index];
}

u32 TexStride(u32 TexU, u32 StrideSel, u32 ScanOrder, u32 MipLevel) {
    if (StrideSel && ScanOrder)
		return (TEXT_CONTROL&31)*32;
    else
        return (8U << TexU) >> MipLevel;
}

u32 DecodeTextel(u32 PixelFmt, u32 PalSelect, u64 memtel, u32 offset) {
    auto memtel_32 = (u32*)&memtel;
    auto memtel_16 = (u16*)&memtel;
    auto memtel_8 = (u8*)&memtel;

    switch (PixelFmt)
    {
        case PixelReserved:
        case Pixel1555:
        case Pixel565:
        case Pixel4444:
        case PixelBumpMap:
            return memtel_16[offset & 3]; break;

        case PixelYUV: {
            auto memtel_yuv = memtel_32[offset & 1];
            auto memtel_yuv8 = (u8*)&memtel_yuv;
            return YUV422(memtel_yuv8[1 + (offset & 2)], memtel_yuv8[0], memtel_yuv8[2]);
            }

        case PixelPal4: {
            auto local_idx = (memtel >> (offset & 15)*4) & 15;
            auto idx = PalSelect * 16 | local_idx;
            return PALETTE_RAM[idx];
        }
        break;
        case PixelPal8: {
            auto local_idx = memtel_8[offset & 7];
            auto idx = (PalSelect / 16) * 256 | local_idx;
            return PALETTE_RAM[idx];
        }
        break;
    }
    return 0xDEADBEEF;
}

u32 GetExpandFormat(u32 PixelFmt) {
    if (PixelFmt == PixelPal4 || PixelFmt == PixelPal8) {
        return PAL_RAM_CTRL&3;
    } else if (PixelFmt == PixelBumpMap || PixelFmt == PixelYUV) {
        return 3;
    } else {
        return PixelFmt & 3;
    }
}
Color MipDebugColor[11] = {
    {.raw = 0xFF000060},
    {.raw = 0xFF000090},
    {.raw = 0xFF0000A0},

    {.raw = 0xFF006000},
    {.raw = 0xFF009000},
    {.raw = 0xFF00A000},

    {.raw = 0xFF600000},
    {.raw = 0xFF900000},
    {.raw = 0xFFA00000},

    {.raw = 0xFFA0A0A0},
    {.raw = 0xFFF0F0F0},
};

static Color TextureFetch(TSP tsp, TCW tcw, int u, int v, u32 MipLevel) {
    
    u32 PixelFmt = tcw.PixelFmt;

    if (MipLevel == (tsp.TexU + 3)) {
        if (PixelFmt == PixelYUV) {
            PixelFmt = Pixel565;
        }
    }

    // These are fixed to zero for pal4/pal8
    u32 ScanOrder = tcw.ScanOrder & ~(PixelFmt == PixelPal4 || PixelFmt == PixelPal8);
    u32 StrideSel = tcw.StrideSel & ~(PixelFmt == PixelPal4 || PixelFmt == PixelPal8);
    
    u32 stride = TexStride(tsp.TexU, StrideSel, ScanOrder, MipLevel);

    u32 start_address = tcw.TexAddr << 3;

    auto fbpp = fBitsPerPixel(tcw);
    
    auto base_address = TexAddressGen(tcw);
    auto offset = TexOffsetGen(tsp, tcw, ScanOrder, u, v, stride, MipLevel);

    u64 memtel = (u64&)emu_vram[(base_address + offset * fbpp / 16) & (VRAM_MASK-7)];

    if (tcw.VQ_Comp) {
        memtel = VQLookup(start_address, memtel, offset * fbpp / 16);
    }

    u32 textel = DecodeTextel(PixelFmt, tcw.PalSelect, memtel, offset);

    u32 expand_format = GetExpandFormat(PixelFmt);

    

    textel = ExpandToARGB8888(textel, expand_format, tcw.ScanOrder);

    // auto old = TextureFetch2(texture, u, v);
    // if (textel != old.raw) {
    //     textel = TextureFetch2(texture, u, v).raw;
    //     textel = TextureFetch(texture, u, v).raw;
    //     die("Missmatch");
    // }
    // auto old = raw_GetTexture(tsp, tcw)[u + v * stride];
    // if (textel != old) {
    //     die("Missmatch");
    // }
    // This uses the old path for debugging
    // return { .raw = raw_GetTexture(tsp, tcw)[u + v * textel_stride] };
    return { .raw =  textel };
    // return MipDebugColor[10-MipLevel];
}
u32 to_u8_256(u8 v) {
    return v + (v >> 7);
}
// Fetch pixels from UVs, interpolate
static Color TextureFilter(
	bool pp_IgnoreTexA,  bool pp_ClampU, bool pp_ClampV, bool pp_FlipU, bool pp_FlipV, u32 pp_FilterMode,
	TSP tsp, TCW tcw, float u, float v, u32 MipLevel, f32 dTrilinear) {
        
    int halfpixel = HALF_OFFSET.texure_pixel_half_offset ? -127 : 0;
    if (MipLevel >= (tsp.TexU + 3)) {
        MipLevel = tsp.TexU+3;
    }
    int sizeU, sizeV;

    if (tcw.MipMapped) {
        sizeU = (8 << tsp.TexU) >> MipLevel;
        sizeV = (8 << tsp.TexU) >> MipLevel;
    } else {
        sizeU = 8 << tsp.TexU;
        sizeV = 8 << tsp.TexV;
    }

    int ui = u * sizeU * 256 + halfpixel;
    int vi = v * sizeV * 256 + halfpixel;

    auto offset00 = TextureFetch(tsp, tcw, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 1, sizeU), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 1, sizeV), MipLevel);
    auto offset01 = TextureFetch(tsp, tcw, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 0, sizeU), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 1, sizeV), MipLevel);
    auto offset10 = TextureFetch(tsp, tcw, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 1, sizeU), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 0, sizeV), MipLevel);
    auto offset11 = TextureFetch(tsp, tcw, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 0, sizeU), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 0, sizeV), MipLevel);

    Color textel = {0xAF674839};

    if (pp_FilterMode == 0) {
        // Point sampling
        for (int i = 0; i < 4; i++)
        {
            textel = offset11;
        }
    } else if (pp_FilterMode == 1) {
        // Bilinear filtering
        int ublend = to_u8_256(ui & 255);
        int vblend = (vi & 255);
        int nublend = 256 - ublend;
        int nvblend = 256 - vblend;

        for (int i = 0; i < 4; i++)
        {
            textel.bgra[i] =
                (offset00.bgra[i] * ublend * vblend) / 65536 +
                (offset01.bgra[i] * nublend * vblend) / 65536 +
                (offset10.bgra[i] * ublend * nvblend) / 65536 +
                (offset11.bgra[i] * nublend * nvblend) / 65536;
        };
    } else {
        // trilinear filtering A and B
        die("pp_FilterMode is trilinear");
    }
        

    if (pp_IgnoreTexA)
    {
        textel.a = 255;
    }

    return textel;
}

// Combine Base, Textel and Offset colors
static Color ColorCombiner(
	bool pp_Texture, bool pp_Offset, u32 pp_ShadInstr,
	Color base, Color textel, Color offset) {

    Color rv = base;
    if (pp_Texture)
    {
        if (pp_ShadInstr == 0)
        {
            //color.rgb = texcol.rgb;
            //color.a = texcol.a;

            rv = textel;
        }
        else if (pp_ShadInstr == 1)
        {
            //color.rgb *= texcol.rgb;
            //color.a = texcol.a;
            for (int i = 0; i < 3; i++)
            {
                rv.bgra[i] = textel.bgra[i] * to_u8_256(base.bgra[i]) / 256;
            }

            rv.a = textel.a;
        }
        else if (pp_ShadInstr == 2)
        {
            //color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
            u32 tb = to_u8_256(textel.a);
            u32 cb = 256 - tb;

            for (int i = 0; i < 3; i++)
            {
                rv.bgra[i] = (textel.bgra[i] * tb + base.bgra[i] * cb) / 256;
            }

            rv.a = base.a;
        }
        else if (pp_ShadInstr == 3)
        {
            //color*=texcol
            for (int i = 0; i < 4; i++)
            {
                rv.bgra[i] = textel.bgra[i] * to_u8_256(base.bgra[i]) / 256;
            }
        }

        if (pp_Offset) {
            // mix only color, saturate
            for (int i = 0; i < 3; i++)
            {
                rv.bgra[i] = std::min(rv.bgra[i] + offset.bgra[i], 255);
            }
        }
    }

    return rv;
}

static Color BumpMapper(Color textel, Color offset) {
    u8 K1 = offset.a;
    u8 K2 = offset.r;
    u8 K3 = offset.g;
    u8 Q = offset.b;

    u8 S = offset.b;
    u8 R = offset.g;
    
    u8 I = u8(K1 + K2*BM_SIN90[S]/256 + K3*BM_COS90[S]*BM_COS360[(R - Q) & 255]/256/256);

	Color res;
	res.b = 255;
	res.g = 255;
	res.r = 255;
	res.a = I;
    return res;
}

// Interpolate the base color, also cheap shadows modifier
static Color InterpolateBase(
	bool pp_UseAlpha, bool pp_CheapShadows,
	const PlaneStepper3* Col, float x, float y, float W, bool InVolume) {
    Color rv;
    u32 mult = 256;

    if (pp_CheapShadows) {
        if (InVolume) {
            mult = to_u8_256(FPU_SHAD_SCALE.scale_factor);
        }
    }

    rv.bgra[0] = 0.5f + Col[0].Ip(x, y, W) * mult / 256;
    rv.bgra[1] = 0.5f + Col[1].Ip(x, y, W) * mult / 256;
    rv.bgra[2] = 0.5f + Col[2].Ip(x, y, W) * mult / 256;
    rv.bgra[3] = 0.5f + Col[3].Ip(x, y, W) * mult / 256;

    if (!pp_UseAlpha)
    {
        rv.a = 255;
    }

    return rv;
}

// Interpolate the offset color, also cheap shadows modifier
static Color InterpolateOffs(bool pp_CheapShadows,
	const PlaneStepper3* Ofs, float x, float y, float W, bool InVolume) {
    Color rv;
    u32 mult = 256;

    if (pp_CheapShadows) {
        if (InVolume) {
            mult = to_u8_256(FPU_SHAD_SCALE.scale_factor);
        }
    }

    rv.bgra[0] = 0.5f + Ofs[0].Ip(x, y, W) * mult / 256;
    rv.bgra[1] = 0.5f + Ofs[1].Ip(x, y, W) * mult / 256;
    rv.bgra[2] = 0.5f + Ofs[2].Ip(x, y, W) * mult / 256;
    rv.bgra[3] = 0.5f + Ofs[3].Ip(x, y, W);

    return rv;
}

// select/calculate blend coefficient for the blend unit
static Color BlendCoefs(
	u32 pp_AlphaInst, bool srcOther,
	Color src, Color dst) {
    Color rv;

    switch(pp_AlphaInst>>1) {
        // zero
        case 0: rv.raw = 0; break;
        // other color
        case 1: rv = srcOther? src : dst; break;
        // src alpha
        case 2: for (int i = 0; i < 4; i++) rv.bgra[i] = src.a; break;
        // dst alpha
        case 3: for (int i = 0; i < 4; i++) rv.bgra[i] = dst.a; break;
    }

    if (pp_AlphaInst & 1) {
        for (int i = 0; i < 4; i++)
            rv.bgra[i] = 255 - rv.bgra[i];
    }

    return rv;
}

// Blending Unit implementation. Alpha blend, accum buffers and such
static bool BlendingUnit(
	bool pp_AlphaTest, u32 pp_SrcSel, u32 pp_DstSel, u32 pp_SrcInst, u32 pp_DstInst,
	u32 index, Color col)
{
    Color rv;
    Color src = {.raw  = pp_SrcSel ? colorBuffer2[index] : col.raw };
    Color dst = {.raw = pp_DstSel ? colorBuffer2[index] : colorBuffer1[index] };
        
    Color src_blend = BlendCoefs(pp_SrcInst, false, src, dst);
    Color dst_blend = BlendCoefs(pp_DstInst, true, src, dst);

    for (int j = 0; j < 4; j++)
    {
        rv.bgra[j] = std::min((src.bgra[j] * to_u8_256(src_blend.bgra[j]) + dst.bgra[j] * to_u8_256(dst_blend.bgra[j])) >> 8, 255U);
    }

    (pp_DstSel ? colorBuffer2[index] : colorBuffer1[index]) = rv.raw;

    if (!pp_AlphaTest || src.a >= PT_ALPHA_REF)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static u8 LookupFogTable(float invW) {
    u8* fog_density=(u8*)&FOG_DENSITY;
    float fog_den_mant=fog_density[1]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
    s32 fog_den_exp=(s8)fog_density[0];
        
    float fog_den = fog_den_mant*powf(2.0f,fog_den_exp);

    f32 fogW = fog_den * invW;

    fogW = std::max((float)fogW, 1.0f);
    fogW = std::min((float)fogW, 255.999985f);

    union f32_fields {
        f32 full;
        struct {
            u32 m: 23;
            u32 e: 8;
            u32 s: 1;
        };
    };

    f32_fields fog_fields = { fogW };

    u32 index = (((fog_fields.e +1) & 7) << 4) |  ((fog_fields.m>>19) & 15);

    u8 blendFactor = (fog_fields.m>>11) & 255;
    u8 blend_inv = 255^blendFactor;

    auto fog_entry = (u8*)&FOG_TABLE[index];

    u8 fog_alpha = (fog_entry[0] * to_u8_256(blendFactor) + fog_entry[1] * to_u8_256(blend_inv)) >> 8;

    return fog_alpha;
}

// Color Clamp and Fog a pixel
static Color FogUnit(bool pp_Offset, bool pp_ColorClamp, u32 pp_FogCtrl, Color col, float invW, u8 offs_a) {
    if (pp_ColorClamp) {
        Color clamp_max = { FOG_CLAMP_MAX };
        Color clamp_min = { FOG_CLAMP_MIN };

        for (int i = 0; i < 4; i++)
        {
            col.bgra[i] = std::min(col.bgra[i], clamp_max.bgra[i]);
            col.bgra[i] = std::max(col.bgra[i], clamp_min.bgra[i]);
        }
    }

    switch(pp_FogCtrl) {
        // Look up mode 1
        case 0b00:
        // look up mode 2
        case 0b11:
            {
                u8 fog_alpha = LookupFogTable(invW);
                    
                u8 fog_inv = 255^fog_alpha;

                Color col_ram = { FOG_COL_RAM };

                if (pp_FogCtrl == 0b00) {
                    for (int i = 0; i < 3; i++) {
                        col.bgra[i] = (col.bgra[i] * to_u8_256(fog_inv) + col_ram.bgra[i] * to_u8_256(fog_alpha))>>8;
                    }
                } else {
                    for (int i = 0; i < 3; i++) {
                        col.bgra[i] = col_ram.bgra[i];
                    }
                    col.a = fog_alpha;
                }
            }
            break;

        // Per Vertex
        case 0b01:
            if (pp_Offset) {
                Color col_vert = { FOG_COL_VERT };
                u8 alpha = offs_a;
                u8 inv = 255^alpha;
                  
                for (int i = 0; i < 3; i++)
                {
                    col.bgra[i] = (col.bgra[i] * to_u8_256(inv) + col_vert.bgra[i] * to_u8_256(alpha))>>8;
                }
            }
            break;

                
        // No Fog
        case 0b10:
            break;
    }

    return col;
}

void DumpTexture(TSP tsp, TCW tcw) {
    char tex_dump[256];
    int max_mipmap = 0;
    int texU = tsp.TexU + 3;
    int texV = tsp.TexV + 3;
    if (tcw.MipMapped) {
        max_mipmap = texU;
        texV = texU;
    }
    for (int mip=0; mip<=max_mipmap; mip++) {
        int width = (1 << texU) >> mip;
        int height = (1 << texV) >> mip;
        snprintf(tex_dump, sizeof(tex_dump), "%s/texture-%08x-%08x-%d-%dx%d.png", dump_textures, tsp.full, tcw.full, mip, height, width);
        Color* tex = new Color[width * height];

        for (int t = 0; t < height; t++) {
            for (int s = 0; s < width; s++) {
                tex[t * width + s] = TextureFetch(tsp, tcw, s, t, mip);
                std::swap(tex[t * width + s].r, tex[t * width + s].b);
            }   
        }

        // stbi_write_png(tex_dump, width, height, 4, tex, width * 4);
        delete[] tex;
    }
}
const char* dump_textures = nullptr;
// Implement the full texture/shade pipeline for a pixel
bool PixelFlush_tsp(
	bool pp_UseAlpha, bool pp_Texture, bool pp_Offset, bool pp_ColorClamp, u32 pp_FogCtrl, bool pp_IgnoreAlpha, bool pp_ClampU, bool pp_ClampV, bool pp_FlipU, bool pp_FlipV, u32 pp_FilterMode, u32 pp_ShadInstr, bool pp_AlphaTest, u32 pp_SrcSel, u32 pp_DstSel, u32 pp_SrcInst, u32 pp_DstInst,
	const FpuEntry *entry, float x, float y, float W, bool InVolume, u32 index)
{
    u32 two_voume_index = InVolume & !FPU_SHAD_SCALE.intensity_shadow;
    auto cb = (Color*)colorBuffer1 + index;
  
    Color base = { 0 }, textel = { 0 }, offs = { 0 };

    base = InterpolateBase(pp_UseAlpha, FPU_SHAD_SCALE.intensity_shadow, entry->ips.Col[two_voume_index], x, y, W, InVolume);

    float dTrilinear;
    u32 MipLevel;
    if (pp_Texture) {
        if (dump_textures) {
            static std::set<u64> dumps;

            u64 uid = entry->params.tsp[two_voume_index].full;
            uid = (uid << 32) | entry->params.tcw[two_voume_index].full;

            if (dumps.count(uid) == 0) {
                dumps.insert(uid);
                DumpTexture(entry->params.tsp[two_voume_index],  entry->params.tcw[two_voume_index]);
            }
        }
        float u = entry->ips.U[two_voume_index].Ip(x, y, W);
        float v = entry->ips.V[two_voume_index].Ip(x, y, W);

        if (entry->params.tcw[two_voume_index].MipMapped) {
            int sizeU = 8 << entry->params.tsp[two_voume_index].TexU;
            // faux mip map cals
            // these really don't follow hw
            float ddx = (entry->ips.U[two_voume_index].ddx + entry->ips.V[two_voume_index].ddx);
            float ddy = (entry->ips.U[two_voume_index].ddy + entry->ips.V[two_voume_index].ddy);

            float dMip = fminf(fabsf(ddx), fabsf(ddy)) * W * sizeU * entry->params.tsp[two_voume_index].MipMapD / 4.0f;

            MipLevel = 0; // biggest
            while(dMip > 1.5 && MipLevel < 11) {
                MipLevel ++;
                dMip = dMip / 2;
            }
            dTrilinear = dMip;
        } else {
            dTrilinear = 0;
            MipLevel = 0;
        }
        
        textel = TextureFilter(pp_IgnoreAlpha, pp_ClampU, pp_ClampV, pp_FlipU, pp_FlipV, pp_FilterMode, entry->params.tsp[two_voume_index], entry->params.tcw[two_voume_index], u, v, MipLevel, dTrilinear);
        if (pp_Offset) {
            offs = InterpolateOffs(FPU_SHAD_SCALE.intensity_shadow, entry->ips.Ofs[two_voume_index], x, y, W, InVolume);
        }
    }

    Color col;
    if (pp_Texture && pp_Offset && entry->params.tcw[two_voume_index].PixelFmt == PixelBumpMap) {
        col = BumpMapper(textel, offs);
    } else {
        col = ColorCombiner(pp_Texture, pp_Offset, pp_ShadInstr, base, textel, offs);
    }
        
    col = FogUnit(pp_Offset, pp_ColorClamp, pp_FogCtrl, col, 1/W, offs.a);

    // if (pp_Texture) {
    //     col = MipDebugColor[10-MipLevel];
    // } else {
    //     col = { .raw = 0 };
    // }
	return BlendingUnit(pp_AlphaTest, pp_SrcSel, pp_DstSel, pp_SrcInst, pp_DstInst, index, col);
}

// Depth processing for a pixel -- render_mode 0: OPAQ, 1: PT, 2: TRANS
void PixelFlush_isp(RenderMode render_mode, u32 depth_mode, u32 ZWriteDis, float x, float y, float invW, u32 index, parameter_tag_t tag)
{
    auto pb = tagBuffer[tagBufferA] + index;
    auto pb2 = tagBuffer[tagBufferB] + index;
    auto zb = depthBuffer[depthBufferA] + index;
    auto zb2 = depthBuffer[depthBufferB] + index;
    auto stencil = stencilBuffer + index;

    auto mode = depth_mode;
        
    if (render_mode == RM_PUNCHTHROUGH)
        mode = 6; // TODO: FIXME
    else if (render_mode == RM_TRANSLUCENT && !ISP_FEED_CFG.pre_sort)
        mode = 3;
    else if (render_mode == RM_MODIFIER)
        mode = 6;
        
    switch(mode) {
        // never
        case 0: return; break;
        // less
        case 1: if (invW >= *zb) return; break;
        // equal
        case 2: if (invW != *zb) return; break;
        // less or equal
        case 3: if (invW > *zb) {
            if (render_mode == RM_TRANSLUCENT && !ISP_FEED_CFG.pre_sort) {
                MoreToDraw = true;
            }
            return;
        }break;
        // greater
        case 4: if (invW <= *zb) return; break;
        // not equal
        case 5: if (invW == *zb) return; break;
        // greater or equal
        case 6: if (invW < *zb) return; break;
        // always
        case 7: break;
    }

    switch (render_mode)
    {
        // OPAQ
        case RM_OPAQUE:
        {
            // Z pre-pass only
            if (!ZWriteDis) {
                *zb = mask_w(invW);
            }
            *pb = tag;
        }
        break;

        case RM_MODIFIER:
        {
            // Flip on Z pass

            *stencil ^= 0b0010;

            // This pixel has valid stencil for summary
            *stencil |= 0b100;
        }
        break;

        // PT
        case RM_PUNCHTHROUGH:
        {
            
            if (invW > *zb2)
                return;

            if (invW == *zb2) {
                auto tagRendered = *pb2 & ~TAG_INVALID;

                if (tag <= tagRendered)
                    return;
            }
            
            *zb = mask_w(invW);
            *pb = tag;
        }
        break;

        // Layer Peeling. zb2 holds the reference depth, zb is used to find closest to reference
        case RM_TRANSLUCENT:
        {
            if (!ISP_FEED_CFG.pre_sort) {
                if (invW < *zb2)
                    return;

                if (invW == *zb2) {
                    auto tagRendered = *pb2 & ~TAG_INVALID;

                    if (tag >= tagRendered)
                        return;
                }

                *zb = mask_w(invW);

                if (!(*pb & TAG_INVALID)) {
                    MoreToDraw = true;
                }
                *pb = tag;
            } else {
                if (!ZWriteDis) {
                    *zb = mask_w(invW);
                }
                *pb = tag;
            }
        }
        break;

        case RM_OP_PT_MV: die("this is invalid here"); break;
    }
}