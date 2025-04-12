#pragma once
#include <cstddef>
#include "components.h"

namespace native {
    struct game_object_t;
}

struct mouse_look_t {
    static constexpr component_type_t componentType = ct_mouse_look;
    native::game_object_t* gameObject;

    size_t playerBodyIndex;

    inline static bool lookEnabled = true;
    inline static bool rotateEnabled = true;

    static constexpr float rotateSpeed = 45.f / 127;

    void update(float deltaTime);
};