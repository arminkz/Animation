#include "ComputePipeline.h"


ComputePipeline::ComputePipeline(std::shared_ptr<VulkanContext> ctx,
                                 const std::string& computeShaderPath,
                                 const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                 const std::vector<VkPushConstantRange>& pushConstantRanges)
    : _ctx(std::move(ctx))
{
    createPipelineLayout(descriptorSetLayouts, pushConstantRanges);
    createComputePipeline(computeShaderPath);
}


ComputePipeline::~ComputePipeline()
{
    vkDestroyPipeline(_ctx->device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_ctx->device, _pipelineLayout, nullptr);
}


void ComputePipeline::bind(VkCommandBuffer commandBuffer)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
}


void ComputePipeline::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                           const std::vector<VkPushConstantRange>& pushConstantRanges)
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

    if (vkCreatePipelineLayout(_ctx->device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
        spdlog::error("Failed to create compute pipeline layout!");
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }
}


void ComputePipeline::createComputePipeline(const std::string& shaderPath)
{
    auto shaderCode = readBinaryFile(shaderPath);
    VkShaderModule shaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = _pipelineLayout;

    if (vkCreateComputePipelines(_ctx->device, _ctx->pipelineCache, 1, &pipelineInfo, nullptr, &_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(_ctx->device, shaderModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline!");
    } else {
        spdlog::info("Compute pipeline created successfully");
    }

    vkDestroyShaderModule(_ctx->device, shaderModule, nullptr);
}


VkShaderModule ComputePipeline::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_ctx->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}


std::vector<char> ComputePipeline::readBinaryFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filename);
        return {};
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
