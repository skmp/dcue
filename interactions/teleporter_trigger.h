#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}

struct teleporter_trigger_t: interaction_t {
    static constexpr component_type_t componentType = ct_teleporter_trigger;

    size_t teleporterToTriggerIndex;
    
    constexpr teleporter_trigger_t(native::game_object_t* owner, int index, bool blocking, size_t teleporterToTriggerIndex)
    : interaction_t{owner, index, blocking}, teleporterToTriggerIndex(teleporterToTriggerIndex) {}
    
    void interact() override;
};