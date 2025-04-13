#pragma once
#include "components.h"

#include <cstddef>

namespace native {
    struct game_object_t;
}

struct timed_activeinactive_t: interaction_t {
    static constexpr component_type_t componentType = ct_timed_activeinactive;

    size_t gameObjectToToggle;
    float delay;
    bool setTo;

    // internal state
    bool triggered;
    float countDown;
    
    constexpr timed_activeinactive_t(native::game_object_t* owner, int index, bool blocking, size_t gameObjectToToggle, float delay, bool setTo)
        : interaction_t{owner, index, blocking}, gameObjectToToggle(gameObjectToToggle), triggered(false), delay(delay), setTo(setTo) {}

    void update(float deltaTime);
    void interact() override;

};