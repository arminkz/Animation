#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/VulkanHelper.h"


class TextureCubemap
{
public:
    TextureCubemap(std::shared_ptr<VulkanContext> ctx, const std::string& path, VkFormat format);
    ~TextureCubemap();

    uint32_t getWidth() const { return _width; }
    uint32_t getHeight() const { return _height; }
    VkImage getImage() const { return _cubemapImage; }
    VkImageView getImageView() const { return _cubemapImageView; }
    VkSampler getSampler() const { return _cubemapSampler; }

    VkDescriptorImageInfo getDescriptorInfo() const;

private:
    std::shared_ptr<VulkanContext> _ctx;

    uint32_t _width;
    uint32_t _height;
    VkFormat _format;
    uint32_t _mipLevels;
    
    VkImage _cubemapImage;
    VkDeviceMemory _cubemapImageMemory;
    VkImageView _cubemapImageView;
    VkSampler _cubemapSampler;
};