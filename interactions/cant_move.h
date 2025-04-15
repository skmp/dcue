#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}

struct cant_move_t: interaction_t {
    static constexpr component_type_t componentType = ct_cant_move;

    bool canMove;
    bool canRotate;
    bool canLook;

    float delay;
    
    constexpr cant_move_t(native::game_object_t* owner, int index, bool blocking, bool canMove, bool canRotate, bool canLook, float delay)
    : interaction_t{owner, index, blocking}, canMove(canMove), canRotate(canRotate), canLook(canLook), delay(delay) {}
    
    void interact() override;
    Task delayDeactivate(float delay);
};