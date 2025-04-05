#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "emu/types.h"

#pragma pack(push, 1) 
union RegionArrayEntryControl {
    struct {
        u32 res0 : 2;
        u32 tilex : 6;
        u32 tiley : 6;
        u32 res1 : 14;
        u32 no_writeout : 1;
        u32 pre_sort : 1;
        u32 z_keep : 1;
        u32 last_region : 1;
    };
    u32 full;
};

typedef u32 pvr32addr_t;
typedef u32 pvr32words_t;
typedef u32 param_offset_words_t;

union ListPointer {
    struct
    {
        u32 pad0 : 2;
        pvr32words_t ptr_in_words : 22;
        u32 pad1 : 7;
        u32 empty : 1;
    };
    u32 full;
};

struct ObjectListTstrip {
    param_offset_words_t param_offs_in_words : 21;
    u32 skip : 3;
    u32 shadow : 1;
    u32 mask : 6;
    u32 is_not_triangle_strip : 1;
};

struct ObjectListTarray {
    param_offset_words_t param_offs_in_words : 21;
    u32 skip : 3;
    u32 shadow : 1;
    u32 prims : 4;
    u32 type : 3;
};

struct ObjectListQarray {
    param_offset_words_t param_offs_in_words : 21;
    u32 skip : 3;
    u32 shadow : 1;
    u32 prims : 4;
    u32 type : 3;
};

struct ObjectListLink {
    u32 pad3 : 2;
    pvr32words_t next_block_ptr_in_words : 22;
    u32 pad4 : 4;
    u32 end_of_list : 1;
    u32 type : 3;
};

union ObjectListEntry {
    struct {
        u32 pad0 : 31;
        u32 is_not_triangle_strip : 1;
    };

    struct {
        u32 pad1 : 29;
        u32 type : 3;
    };

    ObjectListTstrip tstrip;
    ObjectListTarray tarray;
    ObjectListQarray qarray;
    ObjectListLink link;

    u32 full;
};
    
#pragma pack(pop)