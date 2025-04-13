#pragma once

#include "types-common.h"

#include <cstdint>
#include <cstddef>

#include "dc/pvr.h"

#include "../components.h"

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

    struct r_vector3_t {
        union {
            struct {
                float x, y, z;
            };
            struct {
                float r, g, b;
            };
        };
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

    // TODO: proper place
    inline void invertGeneral(r_matrix_t *dst, const r_matrix_t *src)
    {
        float det, invdet;
        // calculate a few cofactors
        dst->right.x = src->up.y*src->at.z - src->up.z*src->at.y;
        dst->right.y = src->at.y*src->right.z - src->at.z*src->right.y;
        dst->right.z = src->right.y*src->up.z - src->right.z*src->up.y;
        // get the determinant from that
        det = src->up.x * dst->right.y + src->at.x * dst->right.z + dst->right.x * src->right.x;
        invdet = 1.0;
        if(det != 0.0f)
            invdet = 1.0f/det;
        dst->right.x *= invdet;
        dst->right.y *= invdet;
        dst->right.z *= invdet;
        dst->up.x = invdet * (src->up.z*src->at.x - src->up.x*src->at.z);
        dst->up.y = invdet * (src->at.z*src->right.x - src->at.x*src->right.z);
        dst->up.z = invdet * (src->right.z*src->up.x - src->right.x*src->up.z);
        dst->at.x = invdet * (src->up.x*src->at.y - src->up.y*src->at.x);
        dst->at.y = invdet * (src->at.x*src->right.y - src->at.y*src->right.x);
        dst->at.z = invdet * (src->right.x*src->up.y - src->right.y*src->up.x);
        dst->pos.x = -(src->pos.x*dst->right.x + src->pos.y*dst->up.x + src->pos.z*dst->at.x);
        dst->pos.y = -(src->pos.x*dst->right.y + src->pos.y*dst->up.y + src->pos.z*dst->at.y);
        dst->pos.z = -(src->pos.x*dst->right.z + src->pos.y*dst->up.z + src->pos.z*dst->at.z);

        dst->at_w = 0;
        dst->up_w = 0;
        dst->right_w = 0;
        dst->pos_w = 1;
    }

    enum game_object_inactive_t {
        goi_active = 0,
        goi_inactive = 1,
        goi_inactive_parent = 2
    };
    
    struct game_object_t {
        r_matrix_t ltw;
        r_vector3_t position;
        r_vector3_t rotation;
        r_vector3_t scale;
        game_object_t* parent;
        size_t* children;
        component_t* components;
        unsigned ltw_stamp;

        mesh_t* mesh;
        material_t** materials;
        size_t submesh_count;
        /*game_object_inactive_t*/ unsigned inactiveFlags;
        bool mesh_enabled;

        bool isActive() const;

        void setActive(bool active);
        void computeActiveState();

        template<typename T>
        T** getComponents() {
            auto componentList = components;
            while (componentList->componentType != ct_eol) {
                auto componentType = componentList->componentType;
                componentList++;
                if (componentType == T::componentType) {
                    return (T**)componentList->data;
                }
                componentList++;
            }
            return nullptr;
        }

        template<typename T>
        T* getComponent() {
            auto components = getComponents<T>();
            if (components) {
                return *components;
            }
            return nullptr;
        }
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