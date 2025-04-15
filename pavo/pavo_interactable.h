#pragma once

#include <cstddef>
#include "components.h"

#include "pavo.h"
#include "dcue/coroutines.h"

#include <set>

namespace native {
    struct game_object_t;
}

struct pavo_interactable_t;

struct pavo_interactabvle_actions_t {
    std::function<Task(pavo_interactable_t*)> onFocus;
    std::function<Task(pavo_interactable_t*)> onInteract;
    std::function<Task(pavo_interactable_t*)> onLookAt;
    std::function<Task(pavo_interactable_t*)> onLookAway;
    std::function<Task(pavo_interactable_t*, native::game_object_t*)> onProximityEnter;
    std::function<Task(pavo_interactable_t*, native::game_object_t*)> onProximityExit;
    float interactionRadius;
};

struct Task;

struct pavo_interactable_t {
    static constexpr component_type_t componentType = ct_pavo_interactable;

    
    native::game_object_t* gameObject;

    std::shared_ptr<pavo_flat_game_env_t> oldEnv;
private:
    pavo_interactabvle_actions_t* nextFocused;
    pavo_interactabvle_actions_t* nextInteract;
    pavo_interactabvle_actions_t* nextLookAt;
    pavo_interactabvle_actions_t* nextLookAway;
    pavo_interactabvle_actions_t* nextProximityEnter;
    pavo_interactabvle_actions_t* nextProximityExit;
    bool looking;

    std::set<native::game_object_t*> inProximity;

public:
    pavo_interactable_t(native::game_object_t* gameObject) : gameObject(gameObject), nextFocused(nullptr), nextInteract(nullptr), nextLookAt(nullptr), nextLookAway(nullptr), nextProximityEnter(nullptr), nextProximityExit(nullptr), looking(false) {}
    
    float getInteractionRadius() {
        if (nextInteract) {
            return nextInteract->interactionRadius;
        } else {
            return 10.f;
        }
    }

    void focused();

    void interact();

    void lookAt();

    void lookAway();

    void onTriggerEnter(native::game_object_t* other);
    
    void onTriggerExit(native::game_object_t* other);

    Task proximityEnter(native::game_object_t* other);

    Task proximityExit(native::game_object_t* other);

    void setNextFocused(pavo_interactabvle_actions_t* next) {
        nextFocused = next;
    }
    void setNextInteract(pavo_interactabvle_actions_t* next) {
        nextInteract = next;
    }
    void setNextLookAt(pavo_interactabvle_actions_t* next) {
        if (nextLookAt == next) {
            return;
        }
        bool wasLooking = looking;
        if (wasLooking) {
            lookAway();
        }
        nextLookAt = next;
        if (wasLooking) {
            lookAt();
        }
    }
    void setNextLookAway(pavo_interactabvle_actions_t* next) {
        if (nextLookAway == next) {
            return;
        }
        bool wasLooking = looking;
        if (wasLooking) {
            lookAway();
        }
        nextLookAway = next;
        if (wasLooking) {
            lookAt();
        }
    }
    void setNextProximityEnter(pavo_interactabvle_actions_t* next) {
        if (nextProximityEnter == next) {
            return;
        }
        for (auto obj : inProximity) {
            proximityExit(obj);
        }
        nextProximityEnter = next;
        for (auto obj : inProximity) {
            proximityEnter(obj);
        }
    }
    void setNextProximityExit(pavo_interactabvle_actions_t* next) {
        if (nextProximityExit == next) {
            return;
        }
        for (auto obj : inProximity) {
            proximityExit(obj);
        }
        nextProximityExit = next;
        for (auto obj : inProximity) {
            proximityEnter(obj);
        }
    }

    // TODO: on destroy?
};