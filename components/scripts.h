#pragma once
// scripts
#include "scripts/proximity_interactable.h"
#include "scripts/player_movement.h"
#include "scripts/mouse_look.h"
#include "scripts/interactable.h"
#include "scripts/teleporter.h"
#include "pavo/pavo_interactable.h"

// interactions
#include "interactions/game_object_activeinactive.h"
#include "interactions/timed_activeinactive.h"
#include "interactions/fadein.h"
#include "interactions/show_message.h"
#include "interactions/teleporter_trigger.h"
#include "interactions/zoom_in_out.h"
#include "interactions/cant_move.h"

// scripts
extern proximity_interactable_t* proximity_interactables[];
extern player_movement_t* player_movements[];
extern mouse_look_t* mouse_looks[];
extern interactable_t* interactables[];
extern teleporter_t* teleporters[];
extern pavo_interactable_t* pavo_interactables[];

// interactions
extern game_object_activeinactive_t* game_object_activeinactives[];
extern timed_activeinactive_t* timed_activeinactives[];
extern fadein_t* fadeins[];
extern show_message_t* show_messages[];
extern teleporter_trigger_t* teleporter_triggers[];
extern zoom_in_out_t* zoom_in_outs[];