#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}
struct audio_clip_t;
struct audio_source_t;

struct play_sound_t: interaction_t {
    static constexpr component_type_t componentType = ct_play_sound;

    audio_clip_t* clip;
    audio_source_t* source;
    
    constexpr play_sound_t(native::game_object_t* owner, int index, bool blocking, audio_clip_t* clip, audio_source_t* source)
    : interaction_t{owner, index, blocking}, clip(clip), source(source) {}
    
    void interact() override;
    Task delayDeactivate(float delay);
};