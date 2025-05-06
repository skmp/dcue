/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "emu/types.h"

#include <algorithm>

extern u32 detwiddle[2][11][1024];
extern s8 BM_SIN90[256];
extern s8 BM_COS90[256];
extern s8 BM_COS360[256];

void BuildTables();


template<class pixel_type>
pixel_type cclamp(pixel_type minv, pixel_type maxv, pixel_type x) {
	return std::min(maxv, std::max(minv, x));
}

// Unpack to 32-bit word

#define ARGB1555_32( word )    ( ((word & 0x8000) ? 0xFF000000 : 0) | (((word>>0) & 0x1F)<<3)  | (((word>>5) & 0x1F)<<11)  | (((word>>10) & 0x1F)<<19) )

#define ARGB565_32( word )     ( (((word>>0)&0x1F)<<3) | (((word>>5)&0x3F)<<10) | (((word>>11)&0x1F)<<19) | 0xFF000000 )

#define ARGB4444_32( word ) ( (((word>>12)&0xF)<<28) | (((word>>0)&0xF)<<4) | (((word>>4)&0xF)<<12) | (((word>>8)&0xF)<<20) )

#define ARGB8888_32( word ) ( word )

static u32 packRGB(u8 R,u8 G,u8 B)
{
	return (R << 0) | (G << 8) | (B << 16) | 0xFF000000;
}

static u32 YUV422(s32 Y,s32 Yu,s32 Yv)
{
	Yu-=128;
	Yv-=128;

	//s32 B = (76283*(Y - 16) + 132252*(Yu - 128))>>16;
	//s32 G = (76283*(Y - 16) - 53281 *(Yv - 128) - 25624*(Yu - 128))>>16;
	//s32 R = (76283*(Y - 16) + 104595*(Yv - 128))>>16;
	
	s32 R = Y + Yv*11/8;            // Y + (Yv-128) * (11/8) ?
	s32 G = Y - (Yu*11 + Yv*22)/32; // Y - (Yu-128) * (11/8) * 0.25 - (Yv-128) * (11/8) * 0.5 ?
	s32 B = Y + Yu*110/64;          // Y + (Yu-128) * (11/8) * 1.25 ?

	return packRGB(cclamp<s32>(0, 255, R),cclamp<s32>(0, 255, G),cclamp<s32>(0, 255, B));
}

#define twop(x,y,bcx,bcy) (detwiddle[0][bcy+3][x]+detwiddle[1][bcx+3][y])

#define twop2(x,y,bcx,bcy) (detwiddle[0][bcy][x]+detwiddle[1][bcx][y])
