#pragma once

#include "stdafx.h"
#include "scene/Model.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/resources/Texture2D.h"
#include "scene/physics/ClothSimulation.h"

class ClothModel : public Model
{
public:
    ClothModel(std::shared_ptr<VulkanContext> ctx, int rows, int cols, float spacing,
               ClothPinMode pinMode = ClothPinMode::LeftColumn,
               glm::mat4 initialTransform = glm::mat4(1.f));

    void draw(VkCommandBuffer commandBuffer, const Renderer& renderer) override;

    void setTexture(const std::string& path);
    const DescriptorSet* getDescriptorSet() const { return _descriptorSet.get(); }

    ClothSimulation* getSimulation() const { return _sim.get(); }

private:
    std::unique_ptr<Texture2D> _texture;
    std::unique_ptr<DescriptorSet> _descriptorSet;
    std::unique_ptr<ClothSimulation> _sim;
};
