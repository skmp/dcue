#pragma once
// scripts
#include "scripts/proximity_interactable.h"
#include "scripts/player_movement.h"
#include "scripts/mouse_look.h"
#include "scripts/interactable.h"

// interactions
#include "interactions/game_object_activeinactive.h"
#include "interactions/timed_activeinactive.h"
#include "interactions/fadein.h"
#include "interactions/show_message.h"

// scripts
extern proximity_interactable_t* proximity_interactables[];
extern player_movement_t* player_movements[];
extern mouse_look_t* mouse_looks[];
extern interactable_t* interactables[];

// interactions
extern game_object_activeinactive_t* game_object_activeinactives[];
extern timed_activeinactive_t* timed_activeinactives[];
extern fadein_t* fadeins[];
extern show_message_t* show_messages[];