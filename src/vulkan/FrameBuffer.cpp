#include "FrameBuffer.h"


FrameBuffer::FrameBuffer(std::shared_ptr<VulkanContext> ctx, const FrameBufferParams& params)
    : _ctx(std::move(ctx)), _extent(params.extent)
{
    if (params.hasColor) {
        if (params.colorImageView != VK_NULL_HANDLE) {
            _colorImageView = params.colorImageView;
        } else {
            _usingInternalColor = true;
            createColorResources(params.extent, params.colorFormat, params.msaaSamples, params.colorUsage);
        }
    }
    
    if (params.hasDepth) {
        if (params.depthImageView != VK_NULL_HANDLE) {
            _depthImageView = params.depthImageView;
        } else {
            _usingInternalDepth = true;
            createDepthResources(params.extent, params.depthFormat, params.msaaSamples, params.depthUsage);
        }
    }

    if (params.hasResolve) {
        if (params.resolveImageView != VK_NULL_HANDLE) {
            _resolveImageView = params.resolveImageView;
        } else {
            _usingInternalResolve = true;
            createResolveResources(params.extent, params.resolveFormat, params.resolveUsage);
        }
    }

    std::vector<VkImageView> attachments;
    if (params.hasColor) {
        attachments.push_back(_colorImageView);
    }
    if (params.hasDepth) {
        attachments.push_back(_depthImageView);
    }
    if (params.hasResolve) {
        attachments.push_back(_resolveImageView);
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = params.renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = params.extent.width;
    framebufferInfo.height = params.extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(_ctx->device, &framebufferInfo, nullptr, &_frameBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer!");
    }
}

FrameBuffer::~FrameBuffer() {
    vkDestroyFramebuffer(_ctx->device, _frameBuffer, nullptr);

    if(_usingInternalColor) {
        vkDestroyImageView(_ctx->device, _colorImageView, nullptr);
        vkDestroyImage(_ctx->device, _colorImage, nullptr);
        vkFreeMemory(_ctx->device, _colorImageMemory, nullptr);
    }

    if(_usingInternalDepth) {
        vkDestroyImageView(_ctx->device, _depthImageView, nullptr);
        vkDestroyImage(_ctx->device, _depthImage, nullptr);
        vkFreeMemory(_ctx->device, _depthImageMemory, nullptr);
    }

    if(_usingInternalResolve) {
        vkDestroyImageView(_ctx->device, _resolveImageView, nullptr);
        vkDestroyImage(_ctx->device, _resolveImage, nullptr);
        vkFreeMemory(_ctx->device, _resolveImageMemory, nullptr);
    }
}

void FrameBuffer::createColorResources(VkExtent2D extent, VkFormat format, VkSampleCountFlagBits msaaSamples, VkImageUsageFlags usage) {

    VulkanHelper::createImage(_ctx, extent.width, extent.height, format, 1, 1, msaaSamples,
        VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _colorImage, _colorImageMemory);

    _colorImageView = VulkanHelper::createImageView(_ctx, _colorImage, format, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
}

void FrameBuffer::createDepthResources(VkExtent2D extent, VkFormat format, VkSampleCountFlagBits msaaSamples, VkImageUsageFlags usage) {

    VulkanHelper::createImage(_ctx, extent.width, extent.height, format, 1, 1, msaaSamples,
        VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _depthImage, _depthImageMemory);

    _depthImageView = VulkanHelper::createImageView(_ctx, _depthImage, format, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void FrameBuffer::createResolveResources(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage) {

    VulkanHelper::createImage(_ctx, extent.width, extent.height, format, 1, 1, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _resolveImage, _resolveImageMemory);

    _resolveImageView = VulkanHelper::createImageView(_ctx, _resolveImage, format, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
}