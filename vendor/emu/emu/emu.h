#pragma once

#include "dc/pvr.h"

extern uint8_t emu_vram[PVR_RAM_SIZE];

void emu_init();
void emu_term();
void emu_pump_events();
void pvr_queue_interrupt(int interrupt);