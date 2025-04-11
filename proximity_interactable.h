#pragma once
#include <cstddef>

namespace native {
    struct game_object_t;
}

struct proximity_interactable_t {
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