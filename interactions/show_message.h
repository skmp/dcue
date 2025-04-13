#pragma once
#include "components.h"
#include "scripts/interactable.h"

namespace native {
    struct game_object_t;
}

struct show_message_t: interaction_t {
    static constexpr component_type_t componentType = ct_show_message;

    const char ** messages;
    
    bool showHasMoreIndicator;
    bool alwaysShowHasMoreIndicator;
    
    const char* speakerName;
    
    bool timedHide;
    bool timedHideCameraLock;
    
    float time;
    bool oneShot;

    // internal state
    int currentMessageIndex;
    float timeToGo;
    
    constexpr show_message_t(native::game_object_t* owner, int index, bool blocking, 
                              const char** messages,
                              bool showHasMoreIndicator, bool alwaysShowHasMoreIndicator,
                              const char* speakerName, bool timedHide, bool timedHideCameraLock,
                              float time, bool oneShot)
        : interaction_t{owner, index, blocking}, messages(messages),
          showHasMoreIndicator(showHasMoreIndicator), alwaysShowHasMoreIndicator(alwaysShowHasMoreIndicator),
          speakerName(speakerName), timedHide(timedHide), timedHideCameraLock(timedHideCameraLock),
          time(time), oneShot(oneShot) {}
    
    void interact() override;
    void update(float deltaTime);

    bool nextMessage();
};