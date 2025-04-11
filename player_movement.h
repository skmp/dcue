#pragma once

namespace native {
    struct game_object_t;
}

struct player_movement_t {
    native::game_object_t* gameObject;

    float speed;
    float gravity;
    float groundDistance;

    inline static bool canMove = true;

    void update(float deltaTime);
};