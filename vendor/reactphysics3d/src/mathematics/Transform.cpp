/********************************************************************************
* ReactPhysics3D physics library, http://www.reactphysics3d.com                 *
* Copyright (c) 2010-2024 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

// Libraries
#include <reactphysics3d/mathematics/Transform.h>
#include <reactphysics3d/mathematics/Matrix3x3.h>

// Namespaces
using namespace reactphysics3d;

// Set the transform from an OpenGL transform matrix
void Transform::setFromOpenGL(decimal* openglMatrix, Vector3& scale) {
    Matrix3x3 matrix(openglMatrix[0], openglMatrix[4], openglMatrix[8],
                     openglMatrix[1], openglMatrix[5], openglMatrix[9],
                     openglMatrix[2], openglMatrix[6], openglMatrix[10]);
    decimal absS[3];
    for(int j = 0; j < 3; ++j){
        absS[j] = std::sqrt(
            matrix[0][j]*matrix[0][j] +
            matrix[1][j]*matrix[1][j] +
            matrix[2][j]*matrix[2][j]
        );
        // avoid divide‑by‑zero
        if(absS[j] < decimal(1e-12)) 
            absS[j] = decimal(1e-12);
    }

    // 3) Build a “pre‑rotation” matrix by normalizing columns
    Matrix3x3 R;
    for(int j = 0; j < 3; ++j){
        R[0][j] = matrix[0][j] / absS[j];
        R[1][j] = matrix[1][j] / absS[j];
        R[2][j] = matrix[2][j] / absS[j];
    }

    // 4) Ensure det(R) = +1.  If det<0, flip the largest scale axis.
    decimal detR =
        R[0][0]*(R[1][1]*R[2][2] - R[1][2]*R[2][1]) -
        R[0][1]*(R[1][0]*R[2][2] - R[1][2]*R[2][0]) +
        R[0][2]*(R[1][0]*R[2][1] - R[1][1]*R[2][0]);

    // start with signed scales = absS
    decimal signedS[3] = { absS[0], absS[1], absS[2] };

    if(detR < 0) {
        // find which axis had the largest magnitude
        int idx = std::distance(
            absS,
            std::max_element(absS, absS+3)
        );
        // flip that column of R, and record the sign
        for(int i = 0; i < 3; ++i)
            R[i][idx] = -R[i][idx];
        signedS[idx] = -signedS[idx];
    }

    // 5) Output the signed scale
    scale.x = signedS[0];
    scale.y = signedS[1];
    scale.z = signedS[2];

    // 6) Convert R → quaternion
    mOrientation = Quaternion(R);
    // mOrientation = Quaternion(matrix);
    // mOrientation.normalize();
    mPosition.setAllValues(openglMatrix[12], openglMatrix[13], openglMatrix[14]);
}

// Get the OpenGL matrix of the transform
void Transform::getOpenGLMatrix(decimal* openglMatrix) const {
    const Matrix3x3& matrix = mOrientation.getMatrix();
    openglMatrix[0] = matrix[0][0]; openglMatrix[1] = matrix[1][0];
    openglMatrix[2] = matrix[2][0]; openglMatrix[3] = 0.0;
    openglMatrix[4] = matrix[0][1]; openglMatrix[5] = matrix[1][1];
    openglMatrix[6] = matrix[2][1]; openglMatrix[7] = 0.0;
    openglMatrix[8] = matrix[0][2]; openglMatrix[9] = matrix[1][2];
    openglMatrix[10] = matrix[2][2]; openglMatrix[11] = 0.0;
    openglMatrix[12] = mPosition.x; openglMatrix[13] = mPosition.y;
    openglMatrix[14] = mPosition.z; openglMatrix[15] = 1.0;
}
