#pragma once

namespace native {
    struct game_object_t;
}

struct fadein_t {
    native::game_object_t* gameObject;

    //TODO: audio source target
    float fadeInDuration;
    float targetVolume;
};