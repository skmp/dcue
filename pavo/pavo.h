#pragma once

#include <functional>
#include <optional>
#include <list>
#include <memory>
#include <cstring>
#include <cassert>

struct pavo_interaction_delegate_t {
    std::function<pavo_interaction_delegate_t()> func;

    pavo_interaction_delegate_t operator()() const {
        return func();
    }

    operator bool() const {
        return func != nullptr;
    }

    pavo_interaction_delegate_t(pavo_interaction_delegate_t(*func)()): func(func) { }
    pavo_interaction_delegate_t(std::function<pavo_interaction_delegate_t()> func): func(func) { }
    pavo_interaction_delegate_t(std::nullptr_t): func(nullptr) {}

    template<typename T>
    pavo_interaction_delegate_t(T func): func(func) { }
    
    pavo_interaction_delegate_t& operator=(pavo_interaction_delegate_t(*func)()) {
        this->func = func;
        return *this;
    }
    pavo_interaction_delegate_t& operator=(std::function<pavo_interaction_delegate_t()> func) {
        this->func = func;
        return *this;
    }

    template<typename T>
    pavo_interaction_delegate_t& operator=(T func) {
        this->func = func;
        return *this;
    }
};

enum pavo_game_mode_t
{
    pgm_MainMenu,
    pgm_Ingame,
    pgm_Cinematic,
    pgm_PauseMenu,
    pgm_MapOverlay,
    pgm_Loading
};

struct pavo_game_env_t {
    std::optional<bool> paused;
    std::optional<bool> canMove;
    std::optional<bool> canRotate;
    std::optional<bool> canLook;
    pavo_interaction_delegate_t onInteraction = nullptr;
    std::optional<bool> cursorVisible;
    std::optional<pavo_game_mode_t> gameMode;
};

struct pavo_flat_game_env_t
{
    bool paused;
    bool canMove;
    bool canRotate;
    bool canLook;
    pavo_interaction_delegate_t onInteraction;
    bool cursorVisible;
    pavo_game_mode_t gameMode;
    pavo_flat_game_env_t(bool paused, bool canMove, bool canRotate, bool canLook, pavo_interaction_delegate_t onInteraction, bool cursorVisible, pavo_game_mode_t gameMode)
        : paused(paused), canMove(canMove), canRotate(canRotate), canLook(canLook), onInteraction(onInteraction), cursorVisible(cursorVisible), gameMode(gameMode) {}
    
    std::shared_ptr<pavo_flat_game_env_t> mergeWith(const pavo_game_env_t& env) const {
        return std::make_shared<pavo_flat_game_env_t>(
            env.paused.value_or(paused),
            env.canMove.value_or(canMove),
            env.canRotate.value_or(canRotate),
            env.canLook.value_or(canLook),
            env.onInteraction ? env.onInteraction : onInteraction,
            env.cursorVisible.value_or(cursorVisible),
            env.gameMode.value_or(gameMode)
        );
    }
};

struct pavo_slot_t {
    const char* name;
    unsigned count;

    bool operator==(const pavo_slot_t& other) const {
        return strcmp(name, other.name) == 0;
    }
};

struct pavo_state_t {
    static std::list<std::shared_ptr<pavo_flat_game_env_t>> envs;
    static std::list<pavo_slot_t> playerState;

    static void pushEnv(const pavo_game_env_t& newEnv, std::shared_ptr<pavo_flat_game_env_t>* old) {
        envs.push_back(getEnv()->mergeWith(newEnv));
        applyState();
        *old = envs.back();
    }

    static void popEnv(std::shared_ptr<pavo_flat_game_env_t>* old) {
        assert(envs.size() > 1);
        envs.remove(*old);
        *old = nullptr;
        applyState();
    }

    static std::shared_ptr<pavo_flat_game_env_t> getEnv() {
        return envs.back();
    }
    
    static void applyState() {
        // TODO
    }

    static void addItem(const char* name) {
        for (auto& slot : playerState) {
            if (strcmp(slot.name, name) == 0) {
                slot.count++;
                return;
            }
        }
        playerState.push_back({ name, 1 });
    }

    static bool hasItemTimes(const char* name, unsigned times) {
        for (auto& slot : playerState) {
            if (strcmp(slot.name, name) == 0 && slot.count >= times) {
                return true;
            }
        }
        return false;
    }

    static bool hasItem(const char* name) {
        return hasItemTimes(name, 1);
    }

    static bool removeItem(const char* name) {
        for (auto& slot : playerState) {
            if (strcmp(slot.name, name) == 0) {
                /* this seems like a bug in the pavo logic, it doesn't take count into account */
                playerState.remove(slot);
                return true;
            }
        }
        return false;
    }


    static void Initialize(const pavo_game_env_t& initialEnv) {
        envs.push_back(std::make_shared<pavo_flat_game_env_t>(
            initialEnv.paused.value_or(false),
            initialEnv.canMove.value_or(true),
            initialEnv.canRotate.value_or(true),
            initialEnv.canLook.value_or(true),
            initialEnv.onInteraction ? initialEnv.onInteraction : nullptr,
            initialEnv.cursorVisible.value_or(true),
            initialEnv.gameMode.value_or(pgm_Ingame)
        ));
    } 
};
