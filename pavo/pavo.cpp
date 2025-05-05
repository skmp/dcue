#include "pavo.h"
#include "pavo_interactable.h"
#include "pavo_units.h"
#include "components/physics.h"

std::list<std::shared_ptr<pavo_flat_game_env_t>> pavo_state_t::envs;
std::list<pavo_slot_t> pavo_state_t::playerState;

pavo_unit_null_t pavo_unit_null_0;

#include "pavo_units.h"

void pavo_interactable_t::focused() {
    if (nextFocused && nextFocused->onFocus) {
        queueCoroutine(nextFocused->onFocus(this));
    }
}

void pavo_interactable_t::interact() {
    if (nextInteract && nextInteract->onInteract) {
        queueCoroutine(nextInteract->onInteract(this));
    }
}

void pavo_interactable_t::lookAt() {
    looking = true;
    if (nextLookAt && nextLookAt->onLookAt) {
        queueCoroutine(nextLookAt->onLookAt(this));
    }
}

void pavo_interactable_t::lookAway() {
    looking = false;
    if (nextLookAway && nextLookAway->onLookAway) {
        queueCoroutine(nextLookAway->onLookAway(this));
    }
}

void pavo_interactable_t::onTriggerEnter(native::game_object_t* other)
{
    inProximity.insert(other);
    queueCoroutine(proximityEnter(other));
}

void pavo_interactable_t::onTriggerExit(native::game_object_t* other)
{
    queueCoroutine(proximityExit(other));
    inProximity.erase(other);
}

Task pavo_interactable_t::proximityEnter(native::game_object_t* other) {
    if (nextProximityEnter && nextProximityEnter->onProximityEnter) {
        return nextProximityEnter->onProximityEnter(this, other);
    } else {
        return pavo_unit_null_0.enter();
    }
}

Task pavo_interactable_t::proximityExit(native::game_object_t* other) {
    if (nextProximityExit && nextProximityExit->onProximityExit) {
        return nextProximityExit->onProximityExit(this, other);
    } else {
        return pavo_unit_null_0.enter();
    }
}

extern std::vector<native::game_object_t*> gameObjects;

Task pavo_unit_set_active_t::enter() {
    if (targetGameObjectIndex != SIZE_MAX) {
        gameObjects[targetGameObjectIndex]->setActive(setTo);
    }
    return onExit->enter();
}

Task pavo_unit_set_enabled_t::enter() {
    if (targetBoxColliderIndex != SIZE_MAX) {
        box_colliders[targetBoxColliderIndex]->enabled = setTo;
        std::cout << "TODO: box_colliders[targetBoxColliderIndex]->enabled = setTo; " << targetBoxColliderIndex << " " << setTo << std::endl;
    }
    return onExit->enter();
}

static bool pavoDone;

static pavo_interaction_delegate_t onInteraction() {
    pavoDone = true;
    return onInteraction;
}

static pavo_interaction_delegate_t ignoreInteraction() {
    return ignoreInteraction;
}

extern const char* messageSpeaker;
extern const char* messageText;

Task PavoTypeMessage(void* context, const char* message, bool hasMore, const char* speaker = nullptr, float timeOut = 0) {
    std::shared_ptr<pavo_flat_game_env_t> oldState;

    if (timeOut == 0)
    {
        pavo_state_t::pushEnv({.onInteraction = onInteraction}, &oldState);
    }
    else
    {
        pavo_state_t::pushEnv({.onInteraction = ignoreInteraction}, &oldState);
    }

    pavoDone = false;
    messageSpeaker = speaker;
    messageText = message;


    co_yield WaitUntil([]() { return pavoDone; });

    messageSpeaker = nullptr;
    messageText = nullptr;

    pavo_state_t::popEnv(&oldState);
}

Task pavo_unit_show_message_t::enter()  {
    if (messages) {
        auto current_message = messages;

        while (*current_message) {
            co_yield PavoTypeMessage(this, current_message[0], current_message[1] != 0, speaker, timeOut);
            current_message++;
        }
    }
    co_yield onExit->enter();
}

Task pavo_unit_audio_source_play_t::enter() {
    std::cout << "TODO: pavo_unit_audio_source_play_t::enter " << std::endl;
    return onExit->enter();
}

Task pavo_unit_mesh_renderer_set_enabled_t::enter() {
    gameObjects[targetGameObjectIndex]->mesh_enabled = setTo;
    return onExit->enter();
}

Task pavo_unit_mesh_collider_set_enabled_t::enter() {
    mesh_colliders[targetMeshColliderIndex]->enabled = setTo;
    return onExit->enter();
}

Task pavo_unit_play_sound_t::enter() {
    std::cout << "TODO: pavo_unit_play_sound_t::enter " << std::endl;
    return onExit->enter();
}