#pragma once
#include "stdafx.h"
#include "VulkanContext.h"
#include "geometry/Vertex.h"

struct PipelineParams
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
    VkRenderPass renderPass;

    // Vertex Bindings and Attributes
    std::optional<VkVertexInputBindingDescription> vertexBindingDescription = Vertex::getBindingDescription();
    std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions = Vertex::getAttributeDescriptions();

    // Vertex and Fragment Shader Specialization Info
    std::optional<VkSpecializationInfo> vertexShaderSpecializationInfo = std::nullopt;
    std::optional<VkSpecializationInfo> fragmentShaderSpecializationInfo = std::nullopt;

    // Input assembly
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool primitiveRestart = false;

    // Rasterization
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Multisampling
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT; // Why the fuck we (by we i mean fucking vulkan) dont read this from renderpass????

    // Depth
    bool depthTest = true;
    bool depthWrite = true;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

    // Color blending
    bool blendEnable = true;

    // Pipeline Name (for debugging purposes)
    std::string name = "";
};

class Pipeline
{
public:
    Pipeline(std::shared_ptr<VulkanContext> ctx, const std::string& vertShaderPath, const std::string& fragShaderPath, const PipelineParams& params);
    ~Pipeline();

    VkPipeline getPipeline() const { return _pipeline; }
    VkPipelineLayout getPipelineLayout() const { return _pipelineLayout; }

    void bind(VkCommandBuffer commandBuffer);

private:
    std::shared_ptr<VulkanContext> _ctx;

    VkPipeline _pipeline = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

    void createPipelineLayout(const PipelineParams& params);
    void createGraphicsPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, const PipelineParams& params);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readBinaryFile(const std::string& filename);

    std::string _name;
};