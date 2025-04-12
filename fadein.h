#pragma once
#include "components.h"

namespace native {
    struct game_object_t;
}

struct fadein_t {
    static constexpr component_type_t componentType = ct_fadein;
    
    native::game_object_t* gameObject;

    //TODO: audio source target
    float fadeInDuration;
    float targetVolume;
};