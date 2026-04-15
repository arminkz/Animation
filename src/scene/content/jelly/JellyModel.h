#pragma once

#include "stdafx.h"
#include "scene/Model.h"
#include "scene/physics/JellySimulation.h"

class JellyModel : public Model
{
public:
    JellyModel(std::shared_ptr<VulkanContext> ctx,
               int resX, int resY, int resZ,
               float spacing, glm::vec3 center);

    void draw(VkCommandBuffer cmd, const Renderer& renderer) override;

    JellySimulation* getSimulation() const { return _sim.get(); }

private:
    std::unique_ptr<JellySimulation> _sim;
};
