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
#include <reactphysics3d/collision/TriangleMesh.h>
#include <vector>
#include <reactphysics3d/utils/Message.h>
#include <reactphysics3d/collision/TriangleVertexArray.h>

using namespace reactphysics3d;

// Constructor
TriangleMesh::TriangleMesh(MemoryAllocator& allocator)
             : mAllocator(allocator), mDynamicAABBTree(allocator), mEpsilon(0) {

}

// // Initialize the mesh using a TriangleVertexArray
// bool TriangleMesh::init(const TriangleVertexArray& triangleVertexArray, std::vector<Message>& messages) {

//     bool isValid = true;

//     // Reserve memory for the vertices, faces and edges
//     mVertices.reserve(triangleVertexArray.getNbVertices());
//     mTriangles.reserve(triangleVertexArray.getNbTriangles() * 3);
//     mVerticesNormals.reserve(triangleVertexArray.getNbVertices());

//     computeEpsilon(triangleVertexArray);

//     // Create the half-edge structure of the mesh
//     isValid &= copyData(triangleVertexArray, messages);

//     // If the normals are not provided by the user
//     if (!triangleVertexArray.getHasNormals() && isValid) {

//         // Compute the normals
//         computeVerticesNormals();
//     }

//     // Insert all the triangles into the dynamic AABB tree
//     initBVHTree();

//     return isValid;
// }

bool TriangleMesh::init(const float* vtx, uint16_t nbVertices, const uint16_t *idx, uint32_t nbTriangles, std::vector<Message>& messages) {

    bool isValid = true;

    // Reserve memory for the vertices, faces and edges
    mVertices = (Vector3*)vtx;
    this->nbVertices = nbVertices;
    mTriangles = idx;
    this->nbTriangles = nbTriangles;


    computeEpsilon();

    // Create the half-edge structure of the mesh
    isValid &= copyData(messages);

    // Insert all the triangles into the dynamic AABB tree
    initBVHTree();

    return isValid;
}

// Compute the epsilon value for this mesh
void TriangleMesh::computeEpsilon() {

    // Compute the bounds of the mesh
    Vector3 max(0, 0, 0);
    for (uint32 i=0 ; i < nbVertices; i++) {

        const Vector3 vertex = mVertices[i];

        decimal maxX = std::abs(vertex.x);
        decimal maxY = std::abs(vertex.y);
        decimal maxZ = std::abs(vertex.z);
        if (maxX > max.x) max.x = maxX;
        if (maxY > max.y) max.y = maxY;
        if (maxZ > max.z) max.z = maxZ;
    }

    // Compute the 'epsilon' value for this set of points
    mEpsilon = 3 * (max.x + max.y + max.z) * MACHINE_EPSILON;
    assert(mEpsilon > 0);
}

// Copy the triangles faces
bool TriangleMesh::copyData(std::vector<Message>& messages) {

    bool isValid = true;

    assert(mEpsilon > 0);

    const decimal epsilonSquare = mEpsilon * mEpsilon;


    // For each face
    for (uint32 i=0 ; i < nbTriangles; i++) {

        bool isValidFace = true;
        uint16 vertexIndices[3] = { mTriangles[i * 3 + 0], mTriangles[i * 3 + 1], mTriangles[i * 3 + 2] };
        Vector3 vertexNormal[3] = {Vector3::zero(), Vector3::zero(), Vector3::zero()};

        if (isValidFace) {

            // Check if the triangle area is not almost zero
            const Vector3 v1 = mVertices[vertexIndices[0]];
            const Vector3 v2 = mVertices[vertexIndices[1]];
            const Vector3 v3 = mVertices[vertexIndices[2]];
            const Vector3 faceNormal = (v3 - v1).cross(v2 - v1);
            const bool isFaceZeroArea = faceNormal.lengthSquare() < epsilonSquare;
            if (isFaceZeroArea) {

                // Add a warning message for the user
                messages.push_back(Message("The face with index " + std::to_string(i) + " has almost zero area. This triangle will not be part of the final collision shape.",
                                           Message::Type::Warning));
            }

            // Check that edges lengths are not almost zero
            decimal edgesLengthsSquare[3];
            edgesLengthsSquare[0] = (v2 - v1).lengthSquare();
            edgesLengthsSquare[1] = (v3 - v2).lengthSquare();
            edgesLengthsSquare[2] = (v1 - v3).lengthSquare();
            bool hasFaceZeroLengthEdge = edgesLengthsSquare[0] < epsilonSquare || edgesLengthsSquare[1] < epsilonSquare ||
                                         edgesLengthsSquare[2] < epsilonSquare;
            if (hasFaceZeroLengthEdge) {

                // Add a warning message for the user
                messages.push_back(Message("The face with index " + std::to_string(i) + " has an almost zero length edge. This triangle will not be part of the final collision shape.",
                                           Message::Type::Warning));
            }

        }
    }

    return isValid;
}


// Insert all the triangles into the dynamic AABB tree
void TriangleMesh::initBVHTree() {

    // TODO : Try to randomly add the triangles into the tree to obtain a better tree

    // For each triangle of the mesh
    for (uint32 f=0; f < nbTriangles; f++) {

        // Get the triangle vertices
        Vector3 trianglePoints[3];
        trianglePoints[0] = mVertices[mTriangles[f * 3]];
        trianglePoints[1] = mVertices[mTriangles[f * 3 + 1]];
        trianglePoints[2] = mVertices[mTriangles[f * 3 + 2]];

        // Create the AABB for the triangle
        AABB aabb = AABB::createAABBForTriangle(trianglePoints);

        // Add the AABB with the index of the triangle into the dynamic AABB tree
        mDynamicAABBTree.addObject(aabb, f);
    }
}

// Return the minimum bounds of the mesh in the x,y,z direction
/**
 * @return The three mimimum bounds of the mesh in the x,y,z direction
 */
const AABB& TriangleMesh::getBounds() const {
    return mDynamicAABBTree.getRootAABB();
}

// Report all shapes overlapping with the AABB given in parameter.
void TriangleMesh::reportAllShapesOverlappingWithAABB(const AABB& aabb, Array<int32>& overlappingNodes) {
    mDynamicAABBTree.reportAllShapesOverlappingWithAABB(aabb, overlappingNodes);
}

// Return the integer data of leaf node of the dynamic AABB tree
int32 TriangleMesh::getDynamicAABBTreeNodeDataInt(int32 nodeID) const {
   return mDynamicAABBTree.getNodeDataInt(nodeID);
}

// Ray casting method
void TriangleMesh::raycast(const Ray& ray, DynamicAABBTreeRaycastCallback& callback) const {
    mDynamicAABBTree.raycast(ray, callback);
}
