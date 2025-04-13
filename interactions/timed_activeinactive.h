#pragma once
#include "components.h"

#include <cstddef>

namespace native {
    struct game_object_t;
}

struct timed_activeinactive_t {
    static constexpr component_type_t componentType = ct_timed_activeinactive;

    native::game_object_t* gameObject;

    size_t gameObjectToToggle;
    float delay;
    bool setTo;

    void update(float deltaTime);
};