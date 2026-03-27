#pragma once

#include "stdafx.h"

#include "vulkan/VulkanContext.h"

// Texture on GPU
class Texture2D
{
public:
    Texture2D(std::shared_ptr<VulkanContext> ctx, const std::string& path, VkFormat format);
    Texture2D(std::shared_ptr<VulkanContext> ctx, const void* pixelData, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels = 0);
    ~Texture2D();

    void cleanup();

    uint32_t getMipLevels() const { return _mipLevels; }
    VkImage getImage() const { return _textureImage; }
    VkImageView getImageView() const { return _textureImageView; }

    VkDescriptorImageInfo getDescriptorInfo() const;

private:
    std::shared_ptr<VulkanContext> _ctx;

    uint32_t _width;
    uint32_t _height;
    VkFormat _format;
    uint32_t _mipLevels;
    
    VkImage _textureImage;
    VkDeviceMemory _textureImageMemory;
    VkImageView _textureImageView;
    VkSampler _textureSampler;
    
    void generateMipmaps();

};