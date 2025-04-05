/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "emu/emu.h"
#include "emu/types.h"

#define VRAM_SIZE PVR_RAM_SIZE
#define VRAM_MASK (VRAM_SIZE - 1)

uint32_t pvr_map32(uint32_t offset32);
f32 vrf(u32 addr);
u32 vri(u32 addr);
u32* vrp(u32 addr);

//read
u16 pvr_read_area1_16(u32 addr);
u32 pvr_read_area1_32(u32 addr);
//write

void pvr_write_area1_16(u32 addr,u16 data);
void pvr_write_area1_32(u32 addr,u32 data);

//registers 
#define PVR_BASE 0x005F8000