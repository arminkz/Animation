#pragma once

#include "scene/physics/MassSpringSimulation.h"

class JellySimulation : public MassSpringSimulation
{
public:
    JellySimulation(std::shared_ptr<VulkanContext> ctx,
                    int resX, int resY, int resZ,
                    float spacing,
                    glm::vec3 center = glm::vec3(0.f));
};
