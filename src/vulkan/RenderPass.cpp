#include "RenderPass.h"

RenderPass::RenderPass(std::shared_ptr<VulkanContext> ctx, RenderPassParams params)
    : _ctx(std::move(ctx))
{
    // Create the render pass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = params.colorFormat;
    colorAttachment.samples = params.msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = params.colorAttachmentLayout;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = params.depthFormat;
    depthAttachment.samples = params.msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = params.depthAttachmentLayout;

    VkAttachmentDescription resolveAttachment{};
    resolveAttachment.format = params.resolveFormat;
    resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resolveAttachment.finalLayout = params.resolveAttachmentLayout;

    std::vector<VkAttachmentDescription> attachments;
    uint32_t attachmentCount = 0;
    uint32_t colorAttachmentIndex = 0;
    uint32_t depthAttachmentIndex = 0;
    uint32_t resolveAttachmentIndex = 0;
    if (params.useColor) {
        attachments.push_back(colorAttachment);
        colorAttachmentIndex = attachmentCount++;
    }
    if (params.useDepth) {
        attachments.push_back(depthAttachment);
        depthAttachmentIndex = attachmentCount++;
    }
    if (params.useResolve) {
        attachments.push_back(resolveAttachment);
        resolveAttachmentIndex = attachmentCount++;
    }

    VkAttachmentReference colorReference = { colorAttachmentIndex, params.colorReferenceLayout };
    VkAttachmentReference depthReference = { depthAttachmentIndex, params.depthReferenceLayout };
    VkAttachmentReference resolveReference = { resolveAttachmentIndex, params.resolveReferenceLayout };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (params.useColor) {
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;
    } else {
        subpass.colorAttachmentCount = 0;
        subpass.pColorAttachments = nullptr;
    }
    if (params.useDepth) {
        subpass.pDepthStencilAttachment = &depthReference; // Attach depth attachment
    } else {
        subpass.pDepthStencilAttachment = nullptr;
    }
    if (params.useResolve) {
        subpass.pResolveAttachments = &resolveReference;   // Attach resolve attachment
    } else {
        subpass.pResolveAttachments = nullptr;
    }

    std::vector<VkSubpassDependency> dependencies;

    if (params.isMultiPass) {
        dependencies.resize(2);
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    } else {
        dependencies.resize(1);
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(_ctx->device, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
    spdlog::info("Render pass created successfully. ({})", params.name);
}


RenderPass::~RenderPass()
{
    vkDestroyRenderPass(_ctx->device, _renderPass, nullptr);
}


