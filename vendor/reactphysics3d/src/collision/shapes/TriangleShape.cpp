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
#include <reactphysics3d/collision/shapes/TriangleShape.h>
#include <reactphysics3d/collision/Collider.h>
#include <reactphysics3d/mathematics/mathematics_functions.h>
#include <reactphysics3d/collision/RaycastInfo.h>
#include <reactphysics3d/utils/Profiler.h>
#include <reactphysics3d/engine/PhysicsCommon.h>
#include <reactphysics3d/configuration.h>
#include <cassert>

using namespace reactphysics3d;


// Constructor for raycasting
TriangleShape::TriangleShape(const Vector3* vertices, uint32 shapeId, HalfEdgeStructure& triangleHalfEdgeStructure, MemoryAllocator& allocator)
    : ConvexPolyhedronShape(CollisionShapeName::TRIANGLE, allocator), mTriangleHalfEdgeStructure(triangleHalfEdgeStructure) {

    mPoints[0] = vertices[0];
    mPoints[1] = vertices[1];
    mPoints[2] = vertices[2];

    mRaycastTestType = TriangleRaycastSide::FRONT;

    mId = shapeId;
}

// Compute the transformed AABB of the collision shape given a transform
/**
 * @param transform Transform to use for the space conversion
 * @return aabb The transformed axis-aligned bounding box (AABB) of the collision shape
 */
AABB TriangleShape::computeTransformedAABB(const Transform& transform) const {

    RP3D_PROFILE("TriangleShape::computeAABB()", mProfiler);

    const Vector3 worldPoint1 = transform * mPoints[0];
    const Vector3 worldPoint2 = transform * mPoints[1];
    const Vector3 worldPoint3 = transform * mPoints[2];

    const Vector3 xAxis(worldPoint1.x, worldPoint2.x, worldPoint3.x);
    const Vector3 yAxis(worldPoint1.y, worldPoint2.y, worldPoint3.y);
    const Vector3 zAxis(worldPoint1.z, worldPoint2.z, worldPoint3.z);

    return AABB(Vector3(xAxis.getMinValue(), yAxis.getMinValue(), zAxis.getMinValue()),
                Vector3(xAxis.getMaxValue(), yAxis.getMaxValue(), zAxis.getMaxValue()));
}

// Raycast method with feedback information
/// This method use the line vs triangle raycasting technique described in
/// Real-time Collision Detection by Christer Ericson.
bool TriangleShape::raycast(const Ray& ray, RaycastInfo& raycastInfo, Collider* collider, MemoryAllocator& /*allocator*/) const {

    RP3D_PROFILE("TriangleShape::raycast()", mProfiler);

    const Vector3 pq = ray.point2 - ray.point1;
    const Vector3 pa = mPoints[0] - ray.point1;
    const Vector3 pb = mPoints[1] - ray.point1;
    const Vector3 pc = mPoints[2] - ray.point1;

    // Test if the line PQ is inside the eges BC, CA and AB. We use the triple
    // product for this test.
    const Vector3 m = pq.cross(pc);
    decimal u = pb.dot(m);
    if (mRaycastTestType == TriangleRaycastSide::FRONT) {
        if (u < decimal(0.0)) return false;
    }
    else if (mRaycastTestType == TriangleRaycastSide::BACK) {
        if (u > decimal(0.0)) return false;
    }

    decimal v = -pa.dot(m);
    if (mRaycastTestType == TriangleRaycastSide::FRONT) {
        if (v < decimal(0.0)) return false;
    }
    else if (mRaycastTestType == TriangleRaycastSide::BACK) {
        if (v > decimal(0.0)) return false;
    }
    else if (mRaycastTestType == TriangleRaycastSide::FRONT_AND_BACK) {
        if (!sameSign(u, v)) return false;
    }

    decimal w = pa.dot(pq.cross(pb));
    if (mRaycastTestType == TriangleRaycastSide::FRONT) {
        if (w < decimal(0.0)) return false;
    }
    else if (mRaycastTestType == TriangleRaycastSide::BACK) {
        if (w > decimal(0.0)) return false;
    }
    else if (mRaycastTestType == TriangleRaycastSide::FRONT_AND_BACK) {
        if (!sameSign(u, w)) return false;
    }

    // If the line PQ is in the triangle plane (case where u=v=w=0)
    if (approxEqual(u, 0) && approxEqual(v, 0) && approxEqual(w, 0)) return false;

    // Compute the barycentric coordinates (u, v, w) to determine the
    // intersection point R, R = u * a + v * b + w * c
    const decimal denom = decimal(1.0) / (u + v + w);
    u *= denom;
    v *= denom;
    w *= denom;

    // Compute the local hit point using the barycentric coordinates
    const Vector3 localHitPoint = u * mPoints[0] + v * mPoints[1] + w * mPoints[2];
    const Vector3 point1ToHitPoint = localHitPoint - ray.point1;
    const decimal hitFraction = point1ToHitPoint.dot(pq) / pq.lengthSquare();

    if (hitFraction < decimal(0.0) || hitFraction > ray.maxFraction) return false;

    // Compute the triangle face normal
    Vector3 normal = (mPoints[1] - mPoints[0]).cross(mPoints[2] - mPoints[0]);
    normal.normalize();
    normal = normal.dot(pq) > decimal(0.0) ? -normal : normal;

    raycastInfo.body = collider->getBody();
    raycastInfo.collider = collider;
    raycastInfo.worldPoint = localHitPoint;
    raycastInfo.hitFraction = hitFraction;
    raycastInfo.worldNormal = normal;

    return true;
}
