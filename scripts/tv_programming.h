#pragma once
#include "components.h"
#include <cstddef>

namespace native {
    struct game_object_t;
}

struct tv_programming_t {
    static constexpr component_type_t componentType = ct_tv_programming;

    native::game_object_t* gameObject;
    float materialRate;
    size_t* materials;
    size_t materialCount;

    float totalTime;
    
    void update(float deltaTime);
};