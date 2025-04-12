#include <cstddef>
#include "components.h"

namespace native {
    struct game_object_t;
}

struct game_object_activeinactive_t {
    static constexpr component_type_t componentType = ct_game_object_activeinactive;

    native::game_object_t* gameObject;

    size_t gameObjectToToggle;
    const char* message;
    bool setTo;

    void interact();
};