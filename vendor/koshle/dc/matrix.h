/* KallistiOS ##version##

   dc/matrix.h
   Copyright (C) 2000 Megan Potter
   Copyright (C) 2013, 2014 Josh "PH3NOM" Pearson
   Copyright (C) 2018 Lawrence Sebald

*/

/** \file    dc/matrix.h
    \brief   Basic matrix operations.
    \ingroup math_matrices

    This file contains various basic matrix math functionality for using the
    SH4's matrix transformation unit. Higher level functionality, like the 3D
    functionality is built off of these operations.

    \see    dc/matrix3d.h

    \author Megan Potter
    \author Josh "PH3NOM" Pearson
*/

#ifndef __DC_MATRIX_H
#define __DC_MATRIX_H

typedef __attribute__ ((aligned (8))) float matrix_t[4][4];

typedef struct vec3f {
    float x, y, z;
} vec3f_t;

typedef struct vectorstr {
    float x, y, z, w;
} vector_t;

extern matrix_t XMTRX;

void mat_store(matrix_t *out);
void mat_load(const matrix_t *out);
void mat_identity(void);
void mat_apply(const matrix_t *src);
void mat_transform(vector_t *invecs, vector_t *outvecs, int veccnt, int vecskip);


void mat_trans_single3_nodiv(float &x, float &y, float &z);

#define mat_trans_single3_nomod(x_, y_, z_, x2, y2, z2) do { \
		vector_t tmp = { x_, y_, z_, 1.0f }; \
		mat_transform(&tmp, &tmp, 1, 0); \
		z2 = 1.0f / tmp.w; \
		x2 = tmp.x * z2; \
		y2 = tmp.y * z2; \
	} while(false)

#endif  /* !__DC_MATRIX_H */