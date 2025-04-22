#pragma once
#include "components.h"

namespace native {
    struct game_object_t;
}
struct interactable_t;

struct interactable_message_t {
    static constexpr component_type_t componentType = ct_interactable_message;

    native::game_object_t* gameObject;
    const char* defaultLookAtText;

    interactable_t* currentLookAtInteractable;
    
    void update(native::game_object_t* mainCamera);
};