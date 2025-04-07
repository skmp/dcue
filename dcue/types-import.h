#pragma once

#include "types-common.h"

#include <cstdint>
#include <cstddef>

namespace import {

    struct texture_t {
        const char* file;
        int width;
        int height;
        const void* data;

        texture_t(const char* file, int width, int height, const void* data)
        {
            this->file = file;
            this->width = width;
            this->height = height;
            this->data = data;
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

    struct submesh_t {
        uint16_t* indices;
        size_t index_count;
    };
    struct mesh_t {
        float* vertices;
        float* uv;
        uint8_t* col;
        float* normals;
        size_t vertex_count;
        submesh_t* submeshes;
        size_t submesh_count;

        mesh_t(size_t submesh_count, submesh_t* submeshes, size_t vertex_count, float* vertices, float* uv, uint8_t* col, float* normals)
        {
            this->submesh_count = submesh_count;
            this->submeshes = submeshes;
            this->vertex_count = vertex_count;
            this->vertices = vertices;
            this->uv = uv;
            this->col = col;
            this->normals = normals;
        }
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
        material_t** materials;
        bool active;
        bool mesh_enabled;

        game_object_t(bool active, matrix_t* transform, bool mesh_enabled, mesh_t* mesh, material_t** materials)
        {
            this->active = active;
            this->transform = transform;
            this->mesh_enabled = mesh_enabled;
            this->mesh = mesh;
            this->materials = materials;
        }
    };
}