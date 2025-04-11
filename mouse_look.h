#pragma once
#include <cstddef>

namespace native {
    struct game_object_t;
}

struct mouse_look_t {
    native::game_object_t* gameObject;

    size_t playerBodyIndex;

    inline static bool lookEnabled = true;
    inline static bool rotateEnabled = true;

    static constexpr float rotateSpeed = 45.f / 127;

    void update(float deltaTime);
};