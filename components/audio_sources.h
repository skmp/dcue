#pragma once

#include <cstdint>

#include "components.h"

struct audio_clip_t;
struct audio_source_t {
    static constexpr component_type_t componentType = ct_audio_source;

    native::game_object_t* gameObject;
    bool enabled;
    audio_clip_t* clip;

    bool playOnAwake;
    float volume;
    float pitch;
    bool loop;
    float spatialBlend;
    float minDistance;
    float maxDistance;

    int playingChannel; // -1 if not playing, sfx or stream id (depends on clip->isSfx) otherwise

    void setEnabled(bool nv);
    void play();
    void awake();
    void disable();

    void update(native::game_object_t* listener);
};

extern audio_source_t* audio_sources[];