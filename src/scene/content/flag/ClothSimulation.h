#pragma once

#include "scene/physics/MassSpringSimulation.h"

class ClothSimulation : public MassSpringSimulation
{
public:
    ClothSimulation(std::shared_ptr<VulkanContext> ctx, int rows, int cols, float spacing);
};
