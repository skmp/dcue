#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}

struct object_dispenser_t: interaction_t {
    static constexpr component_type_t componentType = ct_object_dispenser;


    const char* itemToDispense;
    int itemsAvailable;
    bool deactivateAfterDispensing;

    
    constexpr object_dispenser_t(native::game_object_t* owner, int index, bool blocking, const char* itemToDispense, int itemsAvailable, bool deactivateAfterDispensing)
    : interaction_t{owner, index, blocking}, itemToDispense(itemToDispense), itemsAvailable(itemsAvailable), deactivateAfterDispensing(deactivateAfterDispensing) {}
    
    void interact() override;
};