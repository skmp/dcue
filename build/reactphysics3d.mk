RP3D_OBJECTS = \
	    ../vendor/reactphysics3d/src/body/Body.o \
	    ../vendor/reactphysics3d/src/body/RigidBody.o \
	    ../vendor/reactphysics3d/src/collision/broadphase/DynamicAABBTree.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/CollisionDispatch.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/GJK/VoronoiSimplex.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/GJK/GJKAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/SAT/SATAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/SphereVsSphereAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/CapsuleVsCapsuleAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/SphereVsCapsuleAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/SphereVsConvexPolyhedronAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/CapsuleVsConvexPolyhedronAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/ConvexPolyhedronVsConvexPolyhedronAlgorithm.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/NarrowPhaseInput.o \
	    ../vendor/reactphysics3d/src/collision/narrowphase/NarrowPhaseInfoBatch.o \
	    ../vendor/reactphysics3d/src/collision/shapes/AABB.o \
	    ../vendor/reactphysics3d/src/collision/shapes/ConvexShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/ConvexPolyhedronShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/ConcaveShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/BoxShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/CapsuleShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/CollisionShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/ConvexMeshShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/SphereShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/TriangleShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/ConcaveMeshShape.o \
	    ../vendor/reactphysics3d/src/collision/shapes/HeightFieldShape.o \
	    ../vendor/reactphysics3d/src/collision/RaycastInfo.o \
	    ../vendor/reactphysics3d/src/collision/Collider.o \
	    ../vendor/reactphysics3d/src/collision/PolygonVertexArray.o \
	    ../vendor/reactphysics3d/src/collision/VertexArray.o \
	    ../vendor/reactphysics3d/src/collision/TriangleMesh.o \
	    ../vendor/reactphysics3d/src/collision/HeightField.o \
	    ../vendor/reactphysics3d/src/collision/ConvexMesh.o \
	    ../vendor/reactphysics3d/src/collision/HalfEdgeStructure.o \
	    ../vendor/reactphysics3d/src/collision/ContactManifold.o \
	    ../vendor/reactphysics3d/src/engine/PhysicsCommon.o \
		../vendor/reactphysics3d/src/constraint/ContactPoint.o \
		../vendor/reactphysics3d/src/systems/ContactSolverSystem.o \
	    ../vendor/reactphysics3d/src/systems/DynamicsSystem.o \
	    ../vendor/reactphysics3d/src/systems/CollisionDetectionSystem.o \
	    ../vendor/reactphysics3d/src/engine/PhysicsWorld.o \
	    ../vendor/reactphysics3d/src/engine/Island.o \
	    ../vendor/reactphysics3d/src/engine/Material.o \
	    ../vendor/reactphysics3d/src/engine/OverlappingPairs.o \
	    ../vendor/reactphysics3d/src/engine/Entity.o \
	    ../vendor/reactphysics3d/src/engine/EntityManager.o \
	    ../vendor/reactphysics3d/src/systems/BroadPhaseSystem.o \
	    ../vendor/reactphysics3d/src/components/Components.o \
	    ../vendor/reactphysics3d/src/components/BodyComponents.o \
	    ../vendor/reactphysics3d/src/components/RigidBodyComponents.o \
	    ../vendor/reactphysics3d/src/components/TransformComponents.o \
	    ../vendor/reactphysics3d/src/components/ColliderComponents.o \
	    ../vendor/reactphysics3d/src/collision/CollisionCallback.o \
	    ../vendor/reactphysics3d/src/collision/OverlapCallback.o \
	    ../vendor/reactphysics3d/src/mathematics/Matrix2x2.o \
	    ../vendor/reactphysics3d/src/mathematics/Matrix3x3.o \
	    ../vendor/reactphysics3d/src/mathematics/Quaternion.o \
	    ../vendor/reactphysics3d/src/mathematics/Transform.o \
	    ../vendor/reactphysics3d/src/mathematics/Vector2.o \
	    ../vendor/reactphysics3d/src/mathematics/Vector3.o \
	    ../vendor/reactphysics3d/src/memory/PoolAllocator.o \
	    ../vendor/reactphysics3d/src/memory/SingleFrameAllocator.o \
	    ../vendor/reactphysics3d/src/memory/HeapAllocator.o \
	    ../vendor/reactphysics3d/src/memory/MemoryManager.o \
	    ../vendor/reactphysics3d/src/memory/MemoryAllocator.o \
	    ../vendor/reactphysics3d/src/utils/Profiler.o \
	    ../vendor/reactphysics3d/src/utils/DefaultLogger.o \
	    ../vendor/reactphysics3d/src/utils/DebugRenderer.o \
	    ../vendor/reactphysics3d/src/utils/quickhull/QuickHull.o \
	    ../vendor/reactphysics3d/src/utils/quickhull/QHHalfEdgeStructure.o

RP3D_INCLUDE_DIRS = \
	    -I../vendor/reactphysics3d/include