#pragma once

#include "Math.h"
#include "RenderEngine.h"

class Scene {
public:
    virtual void OnQuit() = 0;
    virtual void OnEntry() = 0;
    virtual void OnExit() = 0;
    virtual void Update(glm::mat4 view_matrix) = 0;
    virtual void Render() = 0;
};
