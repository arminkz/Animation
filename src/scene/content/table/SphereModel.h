#pragma once

#include "stdafx.h"
#include "scene/Model.h"

class SphereModel : public Model
{
public:
    SphereModel(std::shared_ptr<VulkanContext> ctx,
                glm::vec3 center, float radius,
                int segments = 32, int rings = 16);

    void draw(VkCommandBuffer cmd, const Renderer& renderer) override;
};
