#pragma once

#include "Math.h"
#include "RenderEngine.h"

class Scene {
public:
    virtual void Shutdown() = 0;
    virtual void OnEntry() = 0;
    virtual void OnExit() = 0;
    virtual void Update(glm::mat4 view_matrix) = 0;
    virtual void Render() = 0;
    virtual void PipelineReset() = 0;
    virtual void PipelineRebuild() = 0;
};
