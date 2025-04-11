#include <vector>

#include "dcue/types-native.h"
using namespace native;

// components
struct animator_t;
struct camera_t;

// scripts
struct proximity_interactable_t;
struct game_object_activeinactive_t;
struct timed_activeinactive_t;
struct fadein_t;
struct player_movement_t;
struct mouse_look_t;

enum component_type_t {
    // components
    ct_animator,
    ct_camera,

    // scripts
    ct_proximity_interactable,
    ct_game_object_activeinactive,
    ct_timed_activeinactive,
    ct_fadein,
    ct_player_movement,
    ct_mouse_look,

    ct_eol = -1
};

struct component_base_t {
    game_object_t* gameObject;
};
struct component_t {
    union {
        component_type_t componentType;

        // components
        component_base_t** components;
        animator_t** animators;

        // scripts
        camera_t** cameras;
        proximity_interactable_t** proximityInteractables;
        game_object_activeinactive_t** gameObjectActiveinactives;
        timed_activeinactive_t** timedActiveinactives;
        fadein_t** fadeins;
        player_movement_t** playerMovements;
        mouse_look_t** mouseLooks;
    };
};

void InitializeComponents(std::vector<game_object_t*> gameObjects);