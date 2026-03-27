#pragma once
#include "stdafx.h"
#include "VulkanContext.h"


struct RenderPassParams
{
    VkFormat colorFormat;
    VkFormat depthFormat;
    VkFormat resolveFormat;

    VkImageLayout colorAttachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkImageLayout depthAttachmentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkImageLayout resolveAttachmentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkImageLayout colorReferenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkImageLayout depthReferenceLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkImageLayout resolveReferenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    bool useDepth = true;
    bool useColor = true;
    bool useResolve = true;

    bool isMultiPass = true;

    VkSampleCountFlagBits msaaSamples;

    std::string name = "RenderPass";
};


class RenderPass
{
public:
    RenderPass(std::shared_ptr<VulkanContext> ctx, RenderPassParams params);
    ~RenderPass();

    VkRenderPass getRenderPass() const { return _renderPass; }

private:
    std::shared_ptr<VulkanContext> _ctx;

    VkRenderPass _renderPass = VK_NULL_HANDLE;
};