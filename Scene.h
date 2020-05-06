#pragma once

#include <SDL2/SDL.h>

#include "Math.h"
#include "RenderEngine.h"

class Scene {
public:
    virtual void OnQuit() = 0;
    virtual void OnEntry() = 0;
    virtual void OnExit() = 0;
    virtual void Update(std::array<bool, SDL_NUM_SCANCODES>& key_state, bool mouse_capture, int mouse_x, int mouse_y) = 0;
    virtual bool EventHandler(const SDL_Event* event) = 0;
    virtual void Render() = 0;
};
