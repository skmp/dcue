/* KallistiOS ##version##

   matrix3d.c
   Copyright (C) 2000-2002 Megan Potter and Jordan DeLong
   Copyright (C) 2014 Josh Pearson

   Some 3D utils to use with the matrix functions
   Based on example code by Marcus Comstedt
*/


#include <cassert>
#include <cstdint>
#include <cstring>
#include <dc/matrix.h>

matrix_t XMTRX = {
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
};

void mat_identity() {
    XMTRX[0][0] = 1.0f; XMTRX[1][0] = 0.0f; XMTRX[2][0] = 0.0f; XMTRX[3][0] = 0.0f;
    XMTRX[0][1] = 0.0f; XMTRX[1][1] = 1.0f; XMTRX[2][1] = 0.0f; XMTRX[3][1] = 0.0f;
    XMTRX[0][2] = 0.0f; XMTRX[1][2] = 0.0f; XMTRX[2][2] = 1.0f; XMTRX[3][2] = 0.0f;
    XMTRX[0][3] = 0.0f; XMTRX[1][3] = 0.0f; XMTRX[2][3] = 0.0f; XMTRX[3][3] = 1.0f;    
}

void mat_load(const matrix_t *mat) {
    memcpy(XMTRX, mat, sizeof(matrix_t));
}

void mat_store(matrix_t *mat) {
    memcpy(mat, XMTRX, sizeof(matrix_t));
}

static void mat_mult(matrix_t dst, const matrix_t &src1, const matrix_t &src2) {
    for(unsigned r = 0; r < 4; ++r) {
        for(unsigned c = 0; c < 4; ++c) {
            dst[r][c] = 0.0f;

            for(unsigned k = 0; k < 4; ++k)
                dst[r][c] += src1[r][k] * src2[k][c];
        }
    }
}

void mat_apply(const matrix_t* mat) {
    matrix_t result;
    mat_mult(result, *mat, XMTRX);
    mat_load(&result);
}

void mat_transform(vector_t *invecs, vector_t *outvecs, int veccnt, int vecskip) {
    for(unsigned v = 0; v < veccnt; ++v) {
        auto offset = v * (sizeof(vector_t) + vecskip);
        auto *in    = reinterpret_cast<vector_t *>(reinterpret_cast<uint8_t *>(invecs) + offset);
        auto *out   = reinterpret_cast<vector_t *>(reinterpret_cast<uint8_t *>(outvecs) + offset);
    
        float x = XMTRX[0][0]*in->x + XMTRX[1][0]*in->y + XMTRX[2][0]*in->z + XMTRX[3][0]*in->w;
        float y = XMTRX[0][1]*in->x + XMTRX[1][1]*in->y + XMTRX[2][1]*in->z + XMTRX[3][1]*in->w;
        float z = XMTRX[0][2]*in->x + XMTRX[1][2]*in->y + XMTRX[2][2]*in->z + XMTRX[3][2]*in->w;
        float w = XMTRX[0][3]*in->x + XMTRX[1][3]*in->y + XMTRX[2][3]*in->z + XMTRX[3][3]*in->w;
    
        out->x = x; out->y = y; out->z = z; out->w = w;
    }
}

void mat_trans_single3_nodiv(float &x, float &y, float &z) {
    vector_t vec = { x, y, z, 1.0f };
    mat_transform(&vec, &vec, 1, 0);
    x = vec.x; y = vec.y; z = vec.z;
}

static matrix_t tr_m __attribute__((aligned(32))) = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
void mat_translate(float x, float y, float z) {
    tr_m[3][0] = x;
    tr_m[3][1] = y;
    tr_m[3][2] = z;
    mat_apply(&tr_m);
}

static matrix_t sc_m  __attribute__((aligned(32))) = {
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
void mat_scale(float xs, float ys, float zs) {
    sc_m[0][0] = xs;
    sc_m[1][1] = ys;
    sc_m[2][2] = zs;
    mat_apply(&sc_m);
}

static matrix_t rx_m __attribute__((aligned(32))) = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
void mat_rotate_x(float r) {
    //__fsincosr(r, rx_m[2][1], rx_m[1][1]);
    rx_m[2][2] = rx_m[1][1];
    rx_m[1][2] = -rx_m[2][1];
    mat_apply(&rx_m);
}

static matrix_t ry_m  __attribute__((aligned(32))) = {
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
void mat_rotate_y(float r) {
   // __fsincosr(r, ry_m[0][2], ry_m[0][0]);
    ry_m[2][2] = ry_m[0][0];
    ry_m[2][0] = -ry_m[0][2];
    mat_apply(&ry_m);
}

static matrix_t rz_m  __attribute__((aligned(32))) = {
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
void mat_rotate_z(float r) {
    //__fsincosr(r, rz_m[1][0], rz_m[0][0]);
    rz_m[1][1] = rz_m[0][0];
    rz_m[0][1] = -rz_m[1][0];
    mat_apply(&rz_m);
}

void mat_rotate(float xr, float yr, float zr) {
    mat_rotate_x(xr);
    mat_rotate_y(yr);
    mat_rotate_z(zr);
}

/* Some #define's so we can keep the nice looking matrices for reference */
#define XCENTER 0.0f
#define YCENTER 0.0f
#define COT_FOVY_2 1.0f
#define ZNEAR 1.0f
#define ZFAR 100.0f

/* Screen view matrix (used to transform to screen space) */
static matrix_t sv_mat = {
    { YCENTER,    0.0f,   0.0f,  0.0f },
    {    0.0f, YCENTER,   0.0f,  0.0f },
    {    0.0f,    0.0f,   1.0f,  0.0f },
    { XCENTER, YCENTER,   0.0f,  1.0f }
};

/* Frustum matrix (does perspective) */
static matrix_t fr_mat = {
    { COT_FOVY_2,       0.0f,                      0.0f,  0.0f },
    {       0.0f, COT_FOVY_2,                      0.0f,  0.0f },
    {       0.0f,       0.0f, (ZFAR + ZNEAR) / (ZNEAR - ZFAR), -1.0f },
    {       0.0f,       0.0f, 2 * ZFAR*ZNEAR / (ZNEAR - ZFAR),  1.0f }
};

void mat_perspective(float xcenter, float ycenter, float cot_fovy_2,
                     float znear, float zfar) {
    /* Setup the screenview matrix */
    sv_mat[0][0] = sv_mat[1][1] = sv_mat[3][1] = ycenter;
    sv_mat[3][0] = xcenter;
    mat_apply(&sv_mat);

    /* Setup the frustum matrix */
    assert((znear - zfar) != 0);
    fr_mat[0][0] = fr_mat[1][1] = cot_fovy_2;
    fr_mat[2][2] = (zfar + znear) / (znear - zfar);
    fr_mat[3][2] = 2 * zfar * znear / (znear - zfar);
    mat_apply(&fr_mat);
}


/* The following lookat code is based heavily on KGL's gluLookAt */

static inline void cross(const vec3f_t *v1, const vec3f_t *v2, vec3f_t *r) {
    r->x = v1->y * v2->z - v1->z * v2->y;
    r->y = v1->z * v2->x - v1->x * v2->z;
    r->z = v1->x * v2->y - v1->y * v2->x;
}

static matrix_t ml __attribute__((aligned(32))) = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
