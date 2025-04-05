#pragma once

#include "types-common.h"

#include <cstdint>
#include <cstddef>

#include "dc/pvr.h"

namespace native {
    struct texture_t {
        uint32_t flags;
        pvr_ptr_t data;
        pvr_ptr_t tex_pointer;
        uint8_t lw;
        uint8_t lh;

        texture_t(uint32_t flags, pvr_ptr_t data, pvr_ptr_t tex_pointer, uint8_t lw, uint8_t lh)
        {
            this->flags = flags;
            this->data = data;
            this->tex_pointer = tex_pointer;
            this->lw = lw;
            this->lh = lh;
        }
    };

    struct material_t {
        float a, r, g, b;
        texture_t* texture;
        material_t(float a, float r, float g, float b, texture_t* texture)
        {
            this->a = a;
            this->r = r;
            this->g = g;
            this->b = b;
            this->texture = texture;
        }
    };

    struct mesh_t {
        Sphere bounding_sphere;
        uint8_t data[0];
    };

    struct matrix_t {
        float m00, m01, m02, m03;
        float m10, m11, m12, m13;
        float m20, m21, m22, m23;
        float m30, m31, m32, m33;
    };

    struct game_object_t {
        matrix_t* transform;
        mesh_t* mesh;
        material_t* material;
        bool active;
        bool mesh_enabled;

        game_object_t(bool active, matrix_t* transform, bool mesh_enabled, mesh_t* mesh, material_t* material)
        {
            this->active = active;
            this->transform = transform;
            this->mesh_enabled = mesh_enabled;
            this->mesh = mesh;
            this->material = material;
        }
    };
}