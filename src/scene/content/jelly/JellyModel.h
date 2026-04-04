#pragma once

#include "stdafx.h"
#include "scene/Model.h"
#include "scene/physics/JellySimulation.h"

class JellyModel : public Model
{
public:
    JellyModel(std::shared_ptr<VulkanContext> ctx, JellySimulation* sim);

    void draw(VkCommandBuffer cmd, const Renderer& renderer) override;

private:
    JellySimulation* _sim; // non-owning
};
