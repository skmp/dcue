#pragma once
#include <cstdint>

#include "dcue/types-native.h"
#include "components.h"

#include "reactphysics3d/reactphysics3d.h"

extern reactphysics3d::PhysicsCommon physicsCommon;
extern reactphysics3d::PhysicsWorld* physicsWorld;

namespace native {
    struct game_object_t;
}


struct box_collider_t {
    static constexpr component_type_t componentType = ct_box_collider;

    native::game_object_t* gameObject;

    V3d center;
    V3d halfSize;

    reactphysics3d::RigidBody* rigidBody;
    reactphysics3d::Collider* collider;
    reactphysics3d::BoxShape* boxShape;
    V3d lastScale;

    void update(float deltaTime);
};

struct sphere_collider_t {
    static constexpr component_type_t componentType = ct_sphere_collider;

    native::game_object_t* gameObject;

    V3d center;
    float radius;

    reactphysics3d::RigidBody* rigidBody;
    reactphysics3d::Collider* collider;
    reactphysics3d::SphereShape* sphereShape;
    V3d lastScale;
    void update(float deltaTime);
};

struct capsule_collider_t {
    static constexpr component_type_t componentType = ct_capsule_collider;

    native::game_object_t* gameObject;

    V3d center;
    float radius;
    float height;

    reactphysics3d::RigidBody* rigidBody;
    reactphysics3d::Collider* collider;
    reactphysics3d::CapsuleShape* capsuleShape;
    V3d lastScale;
    void update(float deltaTime);
};

struct mesh_collider_t {
    static constexpr component_type_t componentType = ct_mesh_collider;

    native::game_object_t* gameObject;

    float* vertices;
    uint16_t* indices;
    uint16_t vertexCount;
    uint16_t indexCount;

    reactphysics3d::TriangleMesh* triangleMesh;
    reactphysics3d::RigidBody* rigidBody;
    reactphysics3d::Collider* collider;
    reactphysics3d::ConcaveMeshShape* meshShape;
    V3d lastScale;
    void update(float deltaTime);
};

// scripts
extern box_collider_t* box_colliders[];
extern sphere_collider_t* sphere_colliders[];
extern capsule_collider_t* capsule_colliders[];
extern mesh_collider_t* mesh_colliders[];