#pragma once

#include <cstddef>

namespace native {
    struct game_object_t;
}

struct timed_activeinactive_t {
    native::game_object_t* gameObject;

    size_t gameObjectToToggle;
    float delay;
    bool setTo;

    void update(float deltaTime);
};