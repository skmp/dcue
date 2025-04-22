#pragma once

#include <cstdint>

#include "components.h"

struct audio_clip_t;
struct audio_source_t {
    native::game_object_t* gameObject;
    audio_clip_t* clip;

    bool playOnAwake;
    float volume;
    float pitch;
    bool loop;
    float spatialBlend;
    float minDistance;
    float maxDistance;
};