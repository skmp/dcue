#pragma once

#include <cstddef>
#include "components.h"

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

    void focused();
    void interact();

    private:
    bool showMessage();
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
