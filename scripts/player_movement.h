#pragma once
#include "components.h"
namespace native {
    struct game_object_t;
}

struct player_movement_t {
    static constexpr component_type_t componentType = ct_player_movement;
    
    native::game_object_t* gameObject;

    float speed;
    float gravity;
    float groundDistance;

    inline static bool canMove = true;

    void update(float deltaTime);
};