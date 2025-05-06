/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "emu/types.h"
#include "pvr_regs.h"
#include "TexUtils.h"

#include <cmath>

#ifndef M_PI
#define M_PI	(3.14159267)
#endif

u32 detwiddle[2][11][1024];
s8 BM_SIN90[256];
s8 BM_COS90[256];
s8 BM_COS360[256];

//input : address in the yyyyyxxxxx format
//output : address in the xyxyxyxy format
//U : x resolution , V : y resolution
//twiddle works on 64b words


u32 twiddle_slow(u32 x,u32 y,u32 x_sz,u32 y_sz)
{
	u32 rv=0;//low 2 bits are directly passed  -> needs some misc stuff to work.However
			 //Pvr internally maps the 64b banks "as if" they were twiddled :p

	u32 sh=0;
	x_sz>>=1;
	y_sz>>=1;
	while(x_sz!=0 || y_sz!=0)
	{
		if (y_sz)
		{
			u32 temp=y&1;
			rv|=temp<<sh;

			y_sz>>=1;
			y>>=1;
			sh++;
		}
		if (x_sz)
		{
			u32 temp=x&1;
			rv|=temp<<sh;

			x_sz>>=1;
			x>>=1;
			sh++;
		}
	}	
	return rv;
}

void BuildTables()
{
	for (u32 s=0;s<11;s++)
	{
		u32 x_sz=1024;
		u32 y_sz=1<<s;
		for (u32 i=0;i<x_sz;i++)
		{
			detwiddle[0][s][i]=twiddle_slow(i,0,x_sz,y_sz);
			detwiddle[1][s][i]=twiddle_slow(0,i,y_sz,x_sz);
		}
	}

	for (int i = 0; i < 256; i++) {
		BM_SIN90[i]  = 127 * sinf((i / 256.0f) * (M_PI / 2));
		BM_COS90[i]  = 127 * cosf((i / 256.0f) * (M_PI / 2));
		BM_COS360[i] = 127 * cosf((i / 256.0f) * (2 * M_PI));
	}
	
}