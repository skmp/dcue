#pragma once

#include "pavo.h"
#include "pavo_interactable.h"
#include "dcue/coroutines.h"

extern const char* lookAtMessage;

struct pavo_unit_t {
    virtual Task enter() = 0;
};

struct pavo_unit_null_t: pavo_unit_t {
    Task enter() override {
        co_return;
    }
};

extern pavo_unit_null_t pavo_unit_null_0;

struct pavo_unit_wait_for_interaction_t: pavo_unit_t {
    pavo_unit_t* onInteract;
    pavo_unit_t* onFocus;
    
    pavo_interactable_t* interactable;
    const char* lookAtText;
    
    // internal state
    pavo_interactabvle_actions_t actions;

    Task enter() override {
        pavo_state_t::popEnv(&interactable->oldEnv);
        setCallbacks();

        co_return;
    }

    void setCallbacks() {
        if (actions.onFocus) {
            interactable->setNextFocused(&actions);
        }

        if (actions.onInteract) {
            interactable->setNextInteract(&actions);
        }

        if (actions.onLookAt) {
            interactable->setNextLookAt(&actions);
        }

        if (actions.onLookAway) {
            interactable->setNextLookAway(&actions);
        }

        if (actions.onProximityEnter) {
            interactable->setNextProximityEnter(&actions);
        }

        if (actions.onProximityExit) {
            interactable->setNextProximityExit(&actions);
        }
    }

    static pavo_interaction_delegate_t noopInteraction() {
        return noopInteraction;
    }

    pavo_unit_wait_for_interaction_t(pavo_unit_t* onInteract, pavo_unit_t* onFocus, pavo_interactable_t* interactable, const char* lookAtText, float interactionRadius): onInteract(onInteract), onFocus(onFocus), interactable(interactable), lookAtText(lookAtText) {
        if (onInteract != &pavo_unit_null_0) {
            actions.onInteract = [this](pavo_interactable_t*) { 
                pavo_state_t::pushEnv({.canMove = false, .canRotate = false, .onInteraction = noopInteraction}, &this->interactable->oldEnv);
                return this->onInteract->enter();
            };
        }
        if (onFocus != &pavo_unit_null_0) {
            actions.onFocus = [this](pavo_interactable_t*) {
                pavo_state_t::pushEnv({.canMove = false, .canRotate = false, .onInteraction = noopInteraction}, &this->interactable->oldEnv);
                return this->onFocus->enter();
            };
        }
        if (lookAtText) {
            actions.onLookAt = [this](pavo_interactable_t*) -> Task { lookAtMessage = this->lookAtText; co_return; };
            actions.onLookAway = [this](pavo_interactable_t*) -> Task { lookAtMessage = nullptr; co_return; };
        }

        actions.interactionRadius = interactionRadius;
    }

    void initialize() {
        setCallbacks();
    }
};

struct pavo_unit_show_message_t: pavo_unit_t {
    pavo_unit_t* onExit;

    const char* speaker;
    const char** messages;
    
    float timeOut;

    Task enter() override;

    pavo_unit_show_message_t(pavo_unit_t* onExit, const char* speaker, const char** messages, float timeOut): onExit(onExit), speaker(speaker), messages(messages), timeOut(timeOut) { }
};

struct pavo_unit_has_item_t: pavo_unit_t {
    pavo_unit_t* onYes;
    pavo_unit_t* onNo;
    const char* item;

    Task enter() override {
        if (pavo_state_t::hasItem(item)) {
            return onYes->enter();
        } else {
            return onNo->enter();
        }
    }

    pavo_unit_has_item_t(pavo_unit_t* onYes, pavo_unit_t* onNo, const char* item): onYes(onYes), onNo(onNo), item(item) { }
};

struct pavo_unit_dispense_item_t: pavo_unit_t {
    pavo_unit_t* onExit;
    const char* item;

    Task enter() override {
        pavo_state_t::addItem(item);
        return onExit->enter();
    }

    pavo_unit_dispense_item_t(pavo_unit_t* onExit, const char* item): onExit(onExit), item(item) { }
};

extern const char* choices_prompt;
extern const char** choices_options;
extern int choice_chosen;
extern int choice_current;
struct pavo_unit_show_choice_t: pavo_unit_t {
    pavo_unit_t** onChoices;
    pavo_unit_t* after;
    const char* prompt;
    const char** options;
    Task enter() override {
        choice_chosen = -1;
        choice_current = 0;
        choices_prompt = prompt;
        choices_options = options;

        while (choice_chosen == -1) {
            co_yield Step::Frame;
        }

        choices_prompt = nullptr;
        choices_options = nullptr;

        co_yield onChoices[choice_chosen]->enter();
    }

    pavo_unit_show_choice_t(pavo_unit_t** onChoices, pavo_unit_t* after, const char* prompt, const char** options): onChoices(onChoices), after(after), prompt(prompt), options(options) { }
};

struct pavo_unit_end_interaction_t: pavo_unit_t {
    pavo_interactable_t* interactable;
    Task enter() override {
        pavo_state_t::popEnv(&interactable->oldEnv);
        co_return;
    }
    pavo_unit_end_interaction_t(pavo_interactable_t* interactable): interactable(interactable) { }
};

struct pavo_unit_set_active_t: pavo_unit_t {
    pavo_unit_t* onExit;

    size_t targetGameObjectIndex;
    bool setTo;

    Task enter() override;

    pavo_unit_set_active_t(pavo_unit_t* onExit, size_t targetGameObjectIndex, bool setTo): onExit(onExit), targetGameObjectIndex(targetGameObjectIndex), setTo(setTo) { }
};

struct pavo_unit_set_enabled_t: pavo_unit_t {
    pavo_unit_t* onExit;

    size_t targetBoxColliderIndex;
    bool setTo;

    Task enter() override;

    pavo_unit_set_enabled_t(pavo_unit_t* onExit, size_t targetBoxColliderIndex, bool setTo): onExit(onExit), targetBoxColliderIndex(targetBoxColliderIndex), setTo(setTo) { }
};