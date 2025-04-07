#pragma once

#include "types-common.h"

#include <cstdint>
#include <cstddef>

#include "dc/pvr.h"

namespace native {
    struct texture_t {
        uint32_t flags;
        pvr_ptr_t data;
        uint32_t offs;
        uint8_t lw;
        uint8_t lh;
    };

    struct material_t {
        RGBAf color;
        texture_t* texture;
    };

    struct mesh_t {
        Sphere bounding_sphere;
        uint8_t data[0];
    };

    struct alignas(8) r_matrix_t {
        union {
            struct {
                float m00, m10, m20, m30;
                float m01, m11, m21, m31;
                float m02, m12, m22, m32;
                float m03, m13, m23, m33;
            };

            struct {
                V3d right;
                float right_w;
                V3d up;
                float up_w;
                V3d at;
                float at_w;
                V3d pos;
                float pos_w;
            };
        };
    };

    struct game_object_t {
        r_matrix_t ltw;
        mesh_t* mesh;
        material_t** materials;
        size_t submesh_count;
        bool active;
        bool mesh_enabled;
    };

    struct MeshInfo {
        int16_t meshletCount;
        int16_t meshletOffset;
    };
    static_assert(sizeof(MeshInfo) == 4);
    
    struct MeshletInfo {
        Sphere boundingSphere;
        uint16_t flags;
        int8_t pad;
        int8_t vertexSize;
        uint16_t vertexCount;
        uint16_t indexCount;
        uint32_t vertexOffset;
        uint32_t indexOffset;
        uint32_t skinIndexOffset;
        uint32_t skinWeightOffset;
    };
    static_assert(sizeof(MeshletInfo) == 40); // or 32 if !skin
}