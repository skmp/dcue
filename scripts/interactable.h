#pragma once

#include <cstddef>
#include "components.h"
#include "pavo/pavo.h"

namespace native {
    struct game_object_t;
}

struct interactable_t {
    static constexpr component_type_t componentType = ct_interactable;

    native::game_object_t* gameObject;

    const char* speakerName;
    const char* lookAtMessage;
    const char** messages;
    float interactionRadius;

    // runtime state
    std::shared_ptr<pavo_flat_game_env_t> oldState;
    unsigned inspectionCounter;

    void focused();
    void interact();

    bool showMessage();
    pavo_interaction_delegate_t onInteraction();
};

struct interaction_t {
    static constexpr component_type_t componentType = ct_interaction;

    native::game_object_t* gameObject;

    int index;
    bool blocking;

    virtual void interact() = 0;

    constexpr interaction_t(native::game_object_t* owner, int index, bool blocking)
        : gameObject(owner), index(index), blocking(blocking) {}
};
