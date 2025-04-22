#pragma once
#include <cstdint>

#include "components.h"

struct audio_clip_t {
    bool isSfx;
    uint32_t totalSamples;
    uint32_t sampleRate;
    const char* file;

    uintptr_t sfxData;
};

extern audio_clip_t* audio_clips[];
void InitializeAudioClips();