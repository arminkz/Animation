#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/Pipeline.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/resources/TextureCubemap.h"
#include "core/Renderer.h"
#include "geometry/DeviceMesh.h"
#include "scene/Model.h"


class SkyBox : public Model
{
public:
    SkyBox(std::shared_ptr<VulkanContext> ctx, const std::string& textures_path);
    ~SkyBox();

    void draw(VkCommandBuffer commandBuffer, const Renderer& renderer) override;

    const DescriptorSet* getDescriptorSet() const { return _descriptorSet.get(); }

private:
    std::shared_ptr<TextureCubemap> _cubemapTexture;
    std::unique_ptr<DescriptorSet> _descriptorSet;
};