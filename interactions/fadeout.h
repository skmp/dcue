#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}

struct audio_source_t;
struct fadeout_t: interaction_t {
    static constexpr component_type_t componentType = ct_fadein;


    audio_source_t* target;
    float fadeOutDuration;
    float targetVolume;

    
    constexpr fadeout_t(native::game_object_t* owner, int index, bool blocking, audio_source_t* target, float fadeOutDuration, float targetVolume)
    : interaction_t{owner, index, blocking}, target(target), fadeOutDuration(fadeOutDuration), targetVolume(targetVolume) {}
    
    void interact() override;
};