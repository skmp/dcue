#pragma once
#include <cstddef>
#include "components.h"
#include "dcue/coroutines.h"

namespace native {
    struct game_object_t;
}

struct teleporter_t {
    static constexpr component_type_t componentType = ct_teleporter;
    native::game_object_t* gameObject;

    size_t destinationIndex;
    float radius;

    const char* requiresItem;
    bool requiresTrigger;
    bool removeAfterUnlock;

    bool setPosition;
    bool setRotation;

    bool fade;
    float fadeColor[4];
    float fadeInDuraiton;
    float fadeOutDuration;

    void update(float deltaTime);

    void tryTeleport();
    void teleport();

    Task doFade(float fadeInDuration, float fadeOutDuration);
};