#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}

struct fadein_t: interaction_t {
    static constexpr component_type_t componentType = ct_fadein;


    //TODO: audio source target
    float fadeInDuration;
    float targetVolume;

    
    constexpr fadein_t(native::game_object_t* owner, int index, bool blocking, float fadeInDuration, float targetVolume)
    : interaction_t{owner, index, blocking}, fadeInDuration(fadeInDuration), targetVolume(targetVolume) {}
    
    void interact() override;
};