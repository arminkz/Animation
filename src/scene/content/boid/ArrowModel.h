#pragma once

#include "stdafx.h"
#include "scene/Model.h"
#include "vulkan/DescriptorSet.h"

class ArrowModel : public Model
{
public:
    ArrowModel(std::shared_ptr<VulkanContext> ctx);

    void draw(VkCommandBuffer cmd, const Renderer& renderer) override;

    // Set the GPU buffer of mat4 transforms (written by BoidSimulation).
    void setTransformBuffer(VkBuffer buf, uint32_t instanceCount);

    // Descriptor set exposing the transform SSBO to the vertex shader (set=1).
    const DescriptorSet* getTransformDescriptorSet() const { return _transformDS.get(); }

private:
    VkBuffer  _transformBuffer  = VK_NULL_HANDLE;
    uint32_t  _instanceCount    = 0;

    // Descriptor set wrapping _transformBuffer as an SSBO for the vertex shader
    std::unique_ptr<DescriptorSet> _transformDS;
};
