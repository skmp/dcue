#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}

struct recusrive_game_object_activeinactive_t: interaction_t {
    static constexpr component_type_t componentType = ct_recusrive_game_object_activeinactive;

    size_t gameObjectToToggle;
    const char* message;
    bool setTo;

    constexpr recusrive_game_object_activeinactive_t(native::game_object_t* owner, int index, bool blocking, size_t gameObjectToToggle, const char* message, bool setTo)
        : interaction_t{owner, index, blocking}, gameObjectToToggle(gameObjectToToggle), message(message), setTo(setTo) {}

    void interact() override;
};