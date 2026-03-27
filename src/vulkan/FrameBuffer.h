#pragma once
#include "stdafx.h"
#include "VulkanHelper.h"
#include "VulkanContext.h"


struct FrameBufferParams
{
    VkExtent2D extent;
    VkRenderPass renderPass;

    bool hasColor = true;
    bool hasDepth = true;
    bool hasResolve = false;

    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageUsageFlags colorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkFormat resolveFormat = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageUsageFlags resolveUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    VkImageView colorImageView = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkImageView resolveImageView = VK_NULL_HANDLE;
};


class FrameBuffer
{
public:
    FrameBuffer(std::shared_ptr<VulkanContext> ctx, const FrameBufferParams& params);
    ~FrameBuffer();

    VkExtent2D getExtent() const { return _extent; }
    VkFramebuffer getFrameBuffer() const { return _frameBuffer; }
    VkImage getColorImage() const { return _colorImage; }
    VkImageView getColorImageView() const { return _colorImageView; }
    VkImage getDepthImage() const { return _depthImage; }
    VkImageView getDepthImageView() const { return _depthImageView; }
    VkImage getResolveImage() const { return _resolveImage; }
    VkImageView getResolveImageView() const { return _resolveImageView; }

private:
    std::shared_ptr<VulkanContext> _ctx;

    VkExtent2D _extent;

    VkImage _colorImage;
    VkDeviceMemory _colorImageMemory;
    VkImageView _colorImageView;
    bool _usingInternalColor = false;
    void createColorResources(VkExtent2D extent, VkFormat format, VkSampleCountFlagBits msaaSamples, VkImageUsageFlags usage);

    VkImage _depthImage;
    VkDeviceMemory _depthImageMemory;
    VkImageView _depthImageView;
    bool _usingInternalDepth = false;
    void createDepthResources(VkExtent2D extent, VkFormat format, VkSampleCountFlagBits msaaSamples, VkImageUsageFlags usage);

    VkImage _resolveImage;
    VkDeviceMemory _resolveImageMemory;
    VkImageView _resolveImageView;
    bool _usingInternalResolve = false;
    void createResolveResources(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage);

    VkFramebuffer _frameBuffer;
};