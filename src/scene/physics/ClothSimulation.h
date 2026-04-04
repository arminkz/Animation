#pragma once

#include "scene/physics/MassSpringSimulation.h"

enum class ClothPinMode { LeftColumn, TopEdge, None };

class ClothSimulation : public MassSpringSimulation
{
public:
    ClothSimulation(std::shared_ptr<VulkanContext> ctx,
                    int rows, int cols, float spacing,
                    ClothPinMode pinMode = ClothPinMode::LeftColumn,
                    glm::mat4 initialTransform = glm::mat4(1.f));
};
