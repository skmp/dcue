/*
	This file is part of libswirl
*/
//#include "license/bsd"

#include <cstdio>

#include "emu/types.h"
#include "pvr_regs.h"

#include "lxdream/tacore.h"

#include "refsw_tile.h"
#include "TexUtils.h"

#include <dc/asic.h>
void pvr_int_handler(uint32 code, void *data);

u8 pvr_regs[pvr_RegSize];

#include "dc/pvr.h"


uint32 pvrRegRead(uint32 addr) {
    assert(addr < 0x8000 && !(addr&3));
    return PvrReg(addr, u32);
}

volatile bool frameDump = false;

void pvrRegWrite(uint32 addr, uint32 data) {
    assert(addr < 0x8000 && !(addr&3));

    // printf("pvrRegWrite: %4X <= %08X\n", addr, data);

    if (addr == ID_addr)
        return;//read only
    if (addr == REVISION_addr)
        return;//read only
    if (addr == TA_YUV_TEX_CNT_addr)
        return;//read only

    if (addr == STARTRENDER_addr)
    {
        if (frameDump) {
            frameDump = false;
            printf("doing dump... likely to not work for 16mb vram ...\n");
            {
                FILE* v0 = fopen("vram.bin", "wb");
                for (size_t i = 0; i < VRAM_SIZE; i += 4)
                {
                    auto v = vri(i);
                    fwrite(&v, sizeof(v), 1, v0);
                }
                fclose(v0);
            }

            {
                FILE* regs = fopen("pvr_regs", "wb");
                fwrite(pvr_regs, sizeof(pvr_regs), 1, regs);
                fclose(regs);
            }
        }
        // //start render
        // rend_start_render(vram);
		RenderCORE();
		pvr_queue_interrupt(ASIC_EVT_PVR_RENDERDONE_TSP);
        return;
    }

    if (addr == TA_LIST_INIT_addr)
    {
        // FIXME: Fuller TA implementation
        // This is needed because of how KOS sets the bgpoly
        // TA_ISP_CURRENT = TA_ISP_BASE;
        if (data >> 31)
        {
        	lxd_ta_init(emu_vram);
        }
    }

    if (addr == SOFTRESET_addr)
    {
        if (data != 0)
        {
            if (data & 1)
            {
				lxd_ta_reset();
			}
            data = 0;
        }
    }

    if (addr == TA_LIST_CONT_addr)
    {
        //a write of anything works ?
        assert(false && "lxdream list_cont?");
    }

    // if (addr == SPG_CONTROL_addr || addr == SPG_LOAD_addr)
    // {
    //     if (PvrReg(addr, u32) != data)
    //     {
    //         PvrReg(addr, u32) = data;
    //         spg->CalculateSync();
    //     }
    //     return;
    // }
    // if (addr == FB_R_CTRL_addr)
    // {
    //     bool vclk_div_changed = (PvrReg(addr, u32) ^ data) & (1 << 23);
    //     PvrReg(addr, u32) = data;
    //     if (vclk_div_changed)
    //         spg->CalculateSync();
    //     return;
    // }
    // if (addr == FB_R_SIZE_addr)
    // {
    //     if (PvrReg(addr, u32) != data)
    //     {
    //         PvrReg(addr, u32) = data;
    //         fb_dirty = false;
    //         pvr_update_framebuffer_watches();
    //     }
    //     return;
    // }
    // if (addr == TA_YUV_TEX_BASE_addr || addr == TA_YUV_TEX_CTRL_addr)
    // {
    //     PvrReg(addr, u32) = data;
    //     YUV_init(asic);
    //     return;
    // }

    // if (addr >= PALETTE_RAM_START_addr && PvrReg(addr, u32) != data)
    // {
    //     pal_needs_update = true;
    // }

    // if (addr >= FOG_TABLE_START_addr && addr <= FOG_TABLE_END_addr && PvrReg(addr, u32) != data)
    // {
    //     fog_needs_update = true;
    // }
    PvrReg(addr, u32) = data;
    
}

void pvrInit() {
    BuildTables();
	ID = 0x17FD11DB;
	REVISION = 0x00000011;
	SOFTRESET = 0x00000007;
	SPG_HBLANK_INT.full = 0x031D0000;
	SPG_VBLANK_INT.full = 0x01500104;
	FPU_PARAM_CFG.full = 0x0007DF77;
	HALF_OFFSET.full = 0x00000007;
	ISP_FEED_CFG.full = 0x00402000;
	SDRAM_REFRESH = 0x00000020;
	SDRAM_ARB_CFG = 0x0000001F;
	SDRAM_CFG = 0x15F28997;
	SPG_HBLANK.full = 0x007E0345;
	SPG_LOAD.full = 0x01060359;
	SPG_VBLANK.full = 0x01500104;
	SPG_WIDTH.full = 0x07F1933F;
	VO_CONTROL.full = 0x00000108;
	VO_STARTX.full = 0x0000009D;
	VO_STARTY.full = 0x00000015;
	SCALER_CTL.full = 0x00000400;
	FB_BURSTCTRL.full = 0x00090639;
	PT_ALPHA_REF = 0x000000FF;
}

void pvr_ta_data(void* data, int size) {
    lxd_ta_write((unsigned char*)data, size);
}

void pvr_int_handler(uint32 code, void *data);
#include <queue>
static std::queue<int> interrupts;
static bool inInterrupt;
void pvr_queue_interrupt(int interrupt) {
	if (inInterrupt == false) {
		inInterrupt = true;
		pvr_int_handler(interrupt, nullptr);
		while(interrupts.size()) {
			auto ni = interrupts.front();
			interrupts.pop();
			pvr_int_handler(ni, nullptr);
		}
		inInterrupt = false;
	} else {
		interrupts.push(interrupt);
	}

}