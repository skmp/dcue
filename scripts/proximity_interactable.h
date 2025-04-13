#pragma once
#include <cstddef>
#include "components.h"
namespace native {
    struct game_object_t;
}

struct proximity_interactable_t {
    static constexpr component_type_t componentType = ct_proximity_interactable;

    native::game_object_t* gameObject;

    // TODO: is this ever really dynamic?
    size_t playaIndex;

    float radius;
    float distance;
    bool multipleShot;

    // state
    bool hasTriggered;

    void update(float deltaTime);
};