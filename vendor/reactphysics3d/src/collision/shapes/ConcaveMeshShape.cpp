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
#include <reactphysics3d/collision/shapes/ConcaveMeshShape.h>
#include <reactphysics3d/memory/MemoryManager.h>
#include <reactphysics3d/collision/RaycastInfo.h>
#include <reactphysics3d/collision/TriangleMesh.h>
#include <reactphysics3d/utils/Profiler.h>
#include <reactphysics3d/containers/Stack.h>

using namespace reactphysics3d;


struct bvh_leaf_t {
    AABB aabb; // V3d min, max
    int16_t* triangles;
};

struct bvh_interm_t {
    AABB aabb; // V3d min, max
    void* left;
    void* right;
};

struct bvh_t {
    void* root;
    int32_t count;
};

// Constructor
ConcaveMeshShape::ConcaveMeshShape(float* vertices, bvh_t* bvh, MemoryAllocator& allocator, HalfEdgeStructure& triangleHalfEdgeStructure, const Vector3& scaling)
                 : ConcaveShape(CollisionShapeName::TRIANGLE_MESH, allocator, scaling), mTriangleHalfEdgeStructure(triangleHalfEdgeStructure) {

    this->vertices = (Vector3*)vertices;
    this->bvh = bvh;
    mRaycastTestType = TriangleRaycastSide::FRONT;

}

// Set the scale of the shape
void ConcaveMeshShape::setScale(const Vector3& scale) {

    ConcaveShape::setScale(scale);
}

// // Return the three vertices coordinates (in the array outTriangleVertices) of a triangle
// void ConcaveMeshShape::getTriangleVertices(uint32 triangleIndex, Vector3& outV1, Vector3& outV2, Vector3& outV3) const {

//     // Get the vertices coordinates of the triangle
//     mTriangleMesh->getTriangleVertices(triangleIndex, outV1, outV2, outV3);

//     // Apply the scaling factor to the vertices
//     outV1 = outV1 * mScale;
//     outV2 = outV2 * mScale;
//     outV3 = outV3 * mScale;
// }

// // Return the indices of the three vertices of a given triangle in the array
// void ConcaveMeshShape::getTriangleVerticesIndices(uint32 triangleIndex, uint32& outV1Index,
//                                                   uint32& outV2Index, uint32& outV3Index) const {

//     mTriangleMesh->getTriangleVerticesIndices(triangleIndex, outV1Index, outV2Index, outV3Index);
// }

// // Return the number of vertices in the mesh
// uint32 ConcaveMeshShape::getNbVertices() const {
//     return mTriangleMesh->getNbVertices();
// }

// // Return the number of triangles in a sub part of the mesh
// uint32 ConcaveMeshShape::getNbTriangles() const {
//     return mTriangleMesh->getNbTriangles();
// }

// // Return the coordinates of a given vertex
// const Vector3 ConcaveMeshShape::getVertex(uint32 vertexIndex) const {
//     assert(vertexIndex < mTriangleMesh->getNbVertices());
//     return mTriangleMesh->getVertex(vertexIndex) * mScale;
// }

// Return the normal of a given vertex
// const Vector3& ConcaveMeshShape::getVertexNormal(uint32 vertexIndex) const {
//     assert(vertexIndex < mTriangleMesh->getNbVertices());
//     return mScaledVerticesNormals[vertexIndex];
// }

// Compute all the triangles of the mesh that are overlapping with the AABB in parameter
void ConcaveMeshShape::computeOverlappingTriangles(const AABB& localAABB, Array<Vector3>& triangleVertices,
                                                   Array<Vector3>& triangleVerticesNormals, Array<uint32>& shapeIds,
                                                   MemoryAllocator& allocator) const {

    RP3D_PROFILE("ConcaveMeshShape::computeOverlappingTriangles()", mProfiler);

    // Scale the input AABB with the inverse scale of the concave mesh (because
    // we store the vertices without scale inside the dynamic AABB tree
    AABB aabb(localAABB);
    aabb.applyScale(Vector3(decimal(1.0) / mScale.x, decimal(1.0) / mScale.y, decimal(1.0) / mScale.z));

    // Compute the nodes of the internal AABB tree that are overlapping with the AABB
    Array<int32> overlappingNodes(allocator, 64);
    // mTriangleMesh->reportAllShapesOverlappingWithAABB(aabb, overlappingNodes);

    const uint32 nbOverlappingNodes = static_cast<uint32>(overlappingNodes.size());

    // Add space in the array of triangles vertices/normals for the new triangles
    triangleVertices.addWithoutInit(nbOverlappingNodes * 3);
    triangleVerticesNormals.addWithoutInit(nbOverlappingNodes * 3);

    // For each overlapping node
    for (uint32 i=0; i < nbOverlappingNodes; i++) {

        // int nodeId = overlappingNodes[i];

        // // Get the node data (triangle index and mesh subpart index)
        // int32 data = mTriangleMesh->getDynamicAABBTreeNodeDataInt(nodeId);

        // // Get the triangle vertices for this node from the concave mesh shape
        // getTriangleVertices(data, triangleVertices[i * 3], triangleVertices[i * 3 + 1], triangleVertices[i * 3 + 2]);

        // // Get the vertices normals of the triangle
        // // getTriangleVerticesNormals(data, triangleVerticesNormals[i * 3], triangleVerticesNormals[i * 3 + 1],
        // //                            triangleVerticesNormals[i * 3 + 2]);

        // // Compute the triangle shape ID
        // shapeIds.add(computeTriangleShapeId(data));
    }
}

// Raycast method with feedback information
/// Note that only the first triangle hit by the ray in the mesh will be returned, even if
/// the ray hits many triangles.
bool ConcaveMeshShape::raycast(const Ray& ray, RaycastInfo& raycastInfo, Collider* collider, MemoryAllocator& allocator) const {

    RP3D_PROFILE("ConcaveMeshShape::raycast()", mProfiler);

    // Apply the concave mesh inverse scale factor because the mesh is stored without scaling
    // inside the dynamic AABB tree
    // const Vector3 inverseScale(decimal(1.0) / mScale.x, decimal(1.0) / mScale.y, decimal(1.0) / mScale.z);
    // Ray scaledRay(ray.point1 * inverseScale, ray.point2 * inverseScale, ray.maxFraction);

    // Create the callback object that will compute ray casting against triangles
    // ConcaveMeshRaycastCallback raycastCallback(*this, collider, raycastInfo, ray, allocator);

#ifdef IS_RP3D_PROFILING_ENABLED

	// Set the profiler
	raycastCallback.setProfiler(mProfiler);

#endif

    // Ask the Dynamic AABB Tree to report all AABB nodes that are hit by the ray.
    // The raycastCallback object will then compute ray casting against the triangles
    // in the hit AABBs. Note that we use the inverse scaled ray here because AABBs of the TriangleMesh
    // are stored without scaling
    // mTriangleMesh->raycast(scaledRay, raycastCallback);

    decimal maxFraction = ray.maxFraction;

    // Compute the inverse ray direction
    const Vector3 rayDirection = ray.point2 - ray.point1;
    const Vector3 rayDirectionInverse(decimal(1.0) / rayDirection.x, decimal(1.0) / rayDirection.y, decimal(1.0) / rayDirection.z);

    union si {
        void* ptr;
        uintptr_t uptr;
        bvh_leaf_t* leaf;
        bvh_interm_t* interm;
        AABB* aabb;
    };
    Stack<si> stack(mColliders.mAllocator, 128);

    uintptr_t interim_start = (uintptr_t)bvh->root;
    uintptr_t interim_size = sizeof(bvh_interm_t) * bvh->count;
    if (bvh->count == 0) {
        interim_start = interim_size = 0;
        stack.push({ bvh->root });
    } else {
        auto interims = (bvh_interm_t*)bvh->root;
        stack.push({ &interims[bvh->count-1] });
    }
    
    decimal smallestHitFraction = ray.maxFraction;
    bool wasAnyHit = false;
    while (stack.size() > 0) {

        // Get the next node in the stack
        auto node = stack.pop();

        // Test if the ray intersects with the current node AABB
        if (!node.aabb->testRayIntersect(ray.point1, rayDirectionInverse, maxFraction)) continue;

        // If the node is a leaf of the tree
        if ((node.uptr - interim_start) > interim_size) {

            // raycast into the triangles
            auto triangle = node.leaf->triangles;
            do {
                Vector3 trianglePoints[3] = {vertices[triangle[0]], vertices[triangle[1]], vertices[triangle[2]]};
                TriangleShape triangleShape(trianglePoints,  0, mTriangleHalfEdgeStructure, mColliders.mAllocator);
                triangleShape.setRaycastTestType(getRaycastTestType());
                RaycastInfo triRaycastInfo;
                bool isTriangleHit = triangleShape.raycast(ray, triRaycastInfo, mColliders[0], mColliders.mAllocator);

                if (isTriangleHit && triRaycastInfo.hitFraction <= smallestHitFraction) {
                    assert(triRaycastInfo.hitFraction >= decimal(0.0));

                    raycastInfo.body = triRaycastInfo.body;
                    raycastInfo.collider = triRaycastInfo.collider;
                    raycastInfo.hitFraction = triRaycastInfo.hitFraction;
                    raycastInfo.worldPoint = triRaycastInfo.worldPoint;
                    raycastInfo.worldNormal = triRaycastInfo.worldNormal;
                    raycastInfo.triangleIndex = 0;

                    smallestHitFraction = triRaycastInfo.hitFraction;
                    wasAnyHit = true;
                }
                triangle += 3;
            } while (*triangle != -1);


            decimal hitFraction = smallestHitFraction;

            // If the user returned a hitFraction of zero, it means that
            // the raycasting should stop here
            if (hitFraction == decimal(0.0)) {
                return true;
            }

            // If the user returned a positive fraction
            if (hitFraction > decimal(0.0)) {

                // We update the maxFraction value and the ray
                // AABB using the new maximum fraction
                if (hitFraction < maxFraction) {
                    maxFraction = hitFraction;
                }
            }

            // If the user returned a negative fraction, we continue
            // the raycasting as if the collider did not exist
        }
        else {  // If the node has children

            // Push its children in the stack of nodes to explore
            stack.push({node.interm->left});
            stack.push({node.interm->right});
        }
    }

    // raycastCallback.raycastTriangles();

    // return raycastCallback.getIsHit();
    return wasAnyHit;
}

// Collect all the AABB nodes that are hit by the ray in the Dynamic AABB Tree
decimal ConcaveMeshRaycastCallback::raycastBroadPhaseShape(int32 nodeId, const Ray& ray) {

    // Add the id of the hit AABB node into
    mHitAABBNodes.add(nodeId);

    return ray.maxFraction;
}

// Raycast all collision shapes that have been collected
void ConcaveMeshRaycastCallback::raycastTriangles() {

//     Array<int32>::Iterator it;
//     decimal smallestHitFraction = mRay.maxFraction;

//     for (it = mHitAABBNodes.begin(); it != mHitAABBNodes.end(); ++it) {

//         // Get the node data (triangle index and mesh subpart index)
//         int32 data = mConcaveMeshShape.getDynamicAABBTreeNodeDataInt(*it);

//         // Get the triangle vertices for this node from the concave mesh shape
//         Vector3 trianglePoints[3];
//         mConcaveMeshShape.getTriangleVertices(data, trianglePoints[0], trianglePoints[1], trianglePoints[2]);

//         // Create a triangle collision shape
//         TriangleShape triangleShape(trianglePoints,  0, mConcaveMeshShape.mTriangleHalfEdgeStructure, mAllocator);
//         triangleShape.setRaycastTestType(mConcaveMeshShape.getRaycastTestType());
		
// #ifdef IS_RP3D_PROFILING_ENABLED

// 		// Set the profiler to the triangle shape
// 		triangleShape.setProfiler(mProfiler);

// #endif

//         // Ray casting test against the collision shape
//         RaycastInfo raycastInfo;
//         bool isTriangleHit = triangleShape.raycast(mRay, raycastInfo, mCollider, mAllocator);

//         // If the ray hit the collision shape
//         if (isTriangleHit && raycastInfo.hitFraction <= smallestHitFraction) {

//             assert(raycastInfo.hitFraction >= decimal(0.0));

//             mRaycastInfo.body = raycastInfo.body;
//             mRaycastInfo.collider = raycastInfo.collider;
//             mRaycastInfo.hitFraction = raycastInfo.hitFraction;
//             mRaycastInfo.worldPoint = raycastInfo.worldPoint;
//             mRaycastInfo.worldNormal = raycastInfo.worldNormal;
//             mRaycastInfo.triangleIndex = data;

//             smallestHitFraction = raycastInfo.hitFraction;
//             mIsHit = true;
//         }
//     }
}

// Return the local bounds of the shape in x, y and z directions.
// This method is used to compute the AABB of the box
/**
 * @param min The minimum bounds of the shape in local-space coordinates
 * @param max The maximum bounds of the shape in local-space coordinates
 */
AABB ConcaveMeshShape::getLocalBounds() const {
    if (bvh->count) {
        auto interims = (bvh_interm_t*)bvh->root;
        return interims[bvh->count-1].aabb;
    } else {
        return *(AABB*)bvh->root;
    }
}

// Return the integer data of leaf node of the dynamic AABB tree
// int32 ConcaveMeshShape::getDynamicAABBTreeNodeDataInt(int32 nodeID) const {
//    return mTriangleMesh->getDynamicAABBTreeNodeDataInt(nodeID);
// }

// Return the string representation of the shape
std::string ConcaveMeshShape::to_string() const {

    std::stringstream ss;

    // ss << "ConcaveMeshShape{" << std::endl;

    // ss << "nbVertices=" << getNbVertices() << std::endl;
    // ss << "nbTriangles=" << getNbTriangles() << std::endl;

    // ss << "vertices=[";

    // // For each triangle of the concave mesh
    // // for (uint32 v=0; v<getNbVertices(); v++) {

    // //     Vector3 vertex = mTriangleMesh->getVertex(v);

    // //     ss << vertex.to_string() << ", ";
    // // }

    // ss << "], " << std::endl;

    // ss << "triangles=[";

    // // For each triangle of the concave mesh
    // // For each triangle of the concave mesh
    // // for (uint32 triangleIndex=0; triangleIndex < getNbTriangles(); triangleIndex++) {

    // //     uint32 indices[3];

    // //     mTriangleMesh->getTriangleVerticesIndices(triangleIndex, indices[0], indices[1], indices[2]);

    // //     ss << "(" << indices[0] << "," << indices[1] << "," << indices[2] << "), ";
    // // }

    // ss << "], " << std::endl;

    // ss << "}" << std::endl;

    return ss.str();
}

#ifdef IS_RP3D_PROFILING_ENABLED

// Set the profiler
void ConcaveMeshShape::setProfiler(Profiler* profiler) {

    CollisionShape::setProfiler(profiler);

    mTriangleMesh->setProfiler(profiler);
}

#endif
