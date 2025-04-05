#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include <functional>
#include "core_structs.h" // for ISP_TSP and co
#include "refsw_lists_regtypes.h"

struct RegionArrayEntry {
    RegionArrayEntryControl control;
    ListPointer opaque;
    ListPointer opaque_mod;
    ListPointer trans;
    ListPointer trans_mod;
    ListPointer puncht;
};

struct DrawParameters
{
    ISP_TSP isp;
    TSP tsp[2];
    TCW tcw[2];
};

enum RenderMode {
    RM_OPAQUE,
    RM_PUNCHTHROUGH,
    RM_OP_PT_MV, // OP and PT with modvol
    RM_TRANSLUCENT,
    RM_MODIFIER,
};

#define TAG_INVALID (1 << 31)

typedef u32 parameter_tag_t;

struct taRECT {
    int left, top, right, bottom;
};
