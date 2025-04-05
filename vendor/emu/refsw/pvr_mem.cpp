/*
	This file was part of libswirl
*/
//#include "license/bsd"

#include "emu/types.h"
#include "pvr_mem.h"

//Regs

//vram 32-64b
alignas(16*1024*1024) u8 emu_vram[VRAM_SIZE];

//read

u16 pvr_read_area1_16(u32 addr)
{
	return *(u16*)&emu_vram[pvr_map32(addr)];
}
u32 pvr_read_area1_32(u32 addr)
{
	return *(u32*)&emu_vram[pvr_map32(addr)];
}

//write

void pvr_write_area1_16(u32 addr,u16 data)
{
	*(u16*)&emu_vram[pvr_map32(addr)]=data;
}
void pvr_write_area1_32(u32 addr,u32 data)
{
	*(u32*)&emu_vram[pvr_map32(addr)] = data;
}

#define VRAM_BANK_BIT 0x400000

uint32_t pvr_map32(uint32_t offset32)
{
	//64b wide bus is achieved by interleaving the banks every 32 bits
	//const u32 bank_bit = VRAM_BANK_BIT;
	const u32 static_bits = (VRAM_MASK - (VRAM_BANK_BIT * 2 - 1)) | 3;
	const u32 offset_bits = (VRAM_BANK_BIT - 1) & ~3;

	u32 bank = (offset32 & VRAM_BANK_BIT) / VRAM_BANK_BIT;

	u32 rv = offset32 & static_bits;

	rv |= (offset32 & offset_bits) * 2;

	rv |= bank * 4;
	
	return rv;
}


f32 vrf(u32 addr)
{
	return *(f32*)&emu_vram[pvr_map32(addr)];
}
u32 vri(u32 addr)
{
	return *(u32*)&emu_vram[pvr_map32(addr)];
}

u32* vrp(u32 addr)
{
	return (u32*)&emu_vram[pvr_map32(addr)];
}