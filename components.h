#pragma once

#include <vector>

namespace native {
    struct game_object_t;
}

// components
struct animator_t;
struct camera_t;
struct audio_source_t;
struct directional_light_t;
struct point_light_t;

// scripts
struct proximity_interactable_t;
struct player_movement_t;
struct mouse_look_t;
struct interactable_t;
struct teleporter_t;
struct pavo_interactable_t;
struct interactable_message_t;
struct tv_programming_t;

// interactions
/*
- ShowMessage
- PlaySound
- gameobjectactiveinactive2
- GameObjectActiveInactive
- ObjectDispenser
- timedactiveinactive
- Fadein
- CantMove
- Fadeout
- TeleporterTrigger
- zoomout
*/
struct game_object_activeinactive_t;
struct timed_activeinactive_t;
struct fadein_t;
struct show_message_t;
struct teleporter_trigger_t;
struct zoom_in_out_t;
struct cant_move_t;
struct play_sound_t;
struct object_dispenser_t;
struct recusrive_game_object_activeinactive_t;
struct fadeout_t;

// interaction list
struct interaction_t;

// physics
struct box_collider_t;
struct sphere_collider_t;
struct capsule_collider_t;
struct mesh_collider_t;

enum component_type_t {
    // components
    ct_animator,
    ct_camera,
    ct_audio_source,
    ct_directional_light,
    ct_point_light,

    // scripts
    ct_proximity_interactable,
    ct_player_movement,
    ct_mouse_look,
    ct_interactable,
    ct_teleporter,
    ct_pavo_interactable,
    ct_interactable_message,
    ct_tv_programming,

    // physics
    ct_box_collider,
    ct_sphere_collider,
    ct_capsule_collider,
    ct_mesh_collider,

    // interactions
    ct_interaction = 10000,
    ct_game_object_activeinactive,
    ct_timed_activeinactive,
    ct_fadein,
    ct_show_message,
    ct_teleporter_trigger,
    ct_zoom_in_out,
    ct_cant_move,
    ct_play_sound,
    ct_object_dispenser,
    ct_recusrive_game_object_activeinactive,
    ct_fadeout,

    ct_eol = -1
};

struct component_base_t {
    native::game_object_t* gameObject;
};
struct component_t {
    union {
        component_type_t componentType;
        void* data;
        
        // components
        component_base_t** components;
        animator_t** animators;
        audio_source_t** audioSources;
        directional_light_t** directionalLights;
        point_light_t** pointLights;

        // scripts
        camera_t** cameras;
        proximity_interactable_t** proximityInteractables;
        player_movement_t** playerMovements;
        mouse_look_t** mouseLooks;
        interactable_t** interactables;
        teleporter_t** teleporters;
        pavo_interactable_t** pavoInteractables;
        interactable_message_t** interactableMessages;
        tv_programming_t** tvProgrammings;

        // interactions
        game_object_activeinactive_t** gameObjectActiveinactives;
        timed_activeinactive_t** timedActiveinactives;
        fadein_t** fadeins;
        show_message_t** showMessages;
        teleporter_trigger_t** teleporterTriggers;
        zoom_in_out_t** zoomInOuts;
        cant_move_t** cantMoves;
        play_sound_t** playSounds;
        object_dispenser_t** objectDispensers;
        recusrive_game_object_activeinactive_t** recusriveGameObjectActiveinactives;
        fadeout_t** fadeouts;

        // interactions list
        interaction_t** interactions;

        // physics
        box_collider_t** boxColliders;
        sphere_collider_t** sphereColliders;
        capsule_collider_t** capsuleColliders;
        mesh_collider_t** meshColliders;
    };
};

void setGameObject(component_type_t type, component_base_t* component, native::game_object_t* gameObject);

void InitializeComponents(std::vector<native::game_object_t*> gameObjects);

// here to handle recusive include
#include "dcue/types-native.h"