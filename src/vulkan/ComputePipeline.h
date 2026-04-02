#pragma once
#include "stdafx.h"
#include "VulkanContext.h"

class ComputePipeline
{
public:
    ComputePipeline(std::shared_ptr<VulkanContext> ctx,
                    const std::string& computeShaderPath,
                    const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                    const std::vector<VkPushConstantRange>& pushConstantRanges = {});
    ~ComputePipeline();

    VkPipeline getPipeline() const { return _pipeline; }
    VkPipelineLayout getPipelineLayout() const { return _pipelineLayout; }

    void bind(VkCommandBuffer commandBuffer);

private:
    std::shared_ptr<VulkanContext> _ctx;

    VkPipeline _pipeline = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

    void createPipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                              const std::vector<VkPushConstantRange>& pushConstantRanges);
    void createComputePipeline(const std::string& shaderPath);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readBinaryFile(const std::string& filename);
};
