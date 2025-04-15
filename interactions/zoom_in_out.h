#pragma once
#include "components.h"
#include "scripts/interactable.h"

#include "dcue/types-native.h"
#include "reactphysics3d/reactphysics3d.h"
#include "dcue/coroutines.h"

struct zoom_in_out_t: interaction_t {
    static constexpr component_type_t componentType = ct_zoom_in_out;


    size_t cameraIndex;
    size_t targetIndex;

    float inactiveDuration = 0.5f;
    float zoomOutDuration = 1;
    float zoomInDuration = 0.6f;
    float startingDistance = 0.2f;

    bool canMove;
    bool canRotate;
    bool canLook;

    reactphysics3d::Vector3 targetPosition;
    reactphysics3d::Quaternion targetRotation;

    reactphysics3d::Vector3 finalPosition;
    reactphysics3d::Quaternion finalRotation;
    
    zoom_in_out_t(native::game_object_t* owner, int index, bool blocking, 
                             size_t cameraIndex, size_t targetIndex,
                             float inactiveDuration, float zoomOutDuration, float zoomInDuration,
                             float startingDistance, bool canMove, bool canRotate, bool canLook)
        : interaction_t{owner, index, blocking}, cameraIndex(cameraIndex), targetIndex(targetIndex),
          inactiveDuration(inactiveDuration), zoomOutDuration(zoomOutDuration), zoomInDuration(zoomInDuration),
          startingDistance(startingDistance), canMove(canMove), canRotate(canRotate), canLook(canLook) {}
    
    void interact() override;
    void update(float deltaTime);

    Task doAnimation();
};