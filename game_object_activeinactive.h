#include <cstddef>

namespace native {
    struct game_object_t;
}

struct game_object_activeinactive_t {
    native::game_object_t* gameObject;

    size_t gameObjectToToggle;
    const char* message;
    bool setTo;

    void interact();
};