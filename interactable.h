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
};