#include "Pipeline.h"
#include "VulkanHelper.h"
#include "geometry/Vertex.h"


Pipeline::Pipeline(std::shared_ptr<VulkanContext> ctx, const std::string& vertShaderPath, const std::string& fragShaderPath, const PipelineParams& params)
    : _ctx(std::move(ctx)), _name(params.name)
{
    createPipelineLayout(params);
    createGraphicsPipeline(vertShaderPath, fragShaderPath, params);
}


Pipeline::~Pipeline()
{
    vkDestroyPipeline(_ctx->device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_ctx->device, _pipelineLayout, nullptr);
}


void Pipeline::bind(VkCommandBuffer commandBuffer)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
}


VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code)
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


void Pipeline::createPipelineLayout(const PipelineParams& params)
{
    // Create the pipeline layout with the descriptor set layout and push constant range
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(params.descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = params.descriptorSetLayouts.data(); // Use the descriptor set layout from the params
    pipelineLayoutInfo.pushConstantRangeCount =  static_cast<uint32_t>(params.pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = params.pushConstantRanges.data(); // Use the push constant range from the params

    if (vkCreatePipelineLayout(_ctx->device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
        spdlog::error("Failed to create pipeline layout!");
        throw std::runtime_error("Failed to create pipeline layout!");
    }
}


void Pipeline::createGraphicsPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, const PipelineParams& params)
{
    // Load the vertex and fragment shaders
    auto vertShaderCode = readBinaryFile(vertShaderPath);
    auto fragShaderCode = readBinaryFile(fragShaderPath);

    // Create shader modules
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // Create vertex shader stage info
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    if (params.vertexShaderSpecializationInfo.has_value()) {
        vertShaderStageInfo.pSpecializationInfo = &params.vertexShaderSpecializationInfo.value();
    }

    // Create fragment shader stage info
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    if (params.fragmentShaderSpecializationInfo.has_value()) {
        fragShaderStageInfo.pSpecializationInfo = &params.fragmentShaderSpecializationInfo.value();
    }

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // 1- Vertex input state
    // This is where we specify the vertex input format (e.g., position, color, texture coordinates, etc.)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (params.vertexBindingDescription.has_value()) {
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &params.vertexBindingDescription.value(); // Use the vertex binding description from the params
    } else {
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // No binding description provided
    }
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(params.vertexAttributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = params.vertexAttributeDescriptions.data();

    // 2- Input assembly state
    // This is where we specify the topology of the vertex data (e.g., triangle list, line list, etc.)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = params.topology;
    inputAssembly.primitiveRestartEnable = params.primitiveRestart ? VK_TRUE : VK_FALSE;

    // 3- Viewport state
    // This is where we specify the viewport and scissor rectangle for rendering
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Geometry shader / Tessellation control shader
    // Not used in this example, but you can add them here if needed

    // 4- Rasterization state
    // This is where we specify how to rasterize the primitives (e.g., line width, culling mode, etc.)
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = params.polygonMode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = params.cullMode;
    rasterizer.frontFace = params.frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 5- Multisampling state (Anti-aliasing)
    // This is where we specify the multisampling settings (e.g., sample count, sample mask, etc.)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.minSampleShading = 0.2f; // Optional
    multisampling.rasterizationSamples = params.msaaSamples;

    // 6- Depth and stencil state
    // This is where we specify the depth and stencil settings (e.g., depth test, depth write, etc.)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = params.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = params.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = params.depthCompareOp; // Lower depth = closer to camera
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    // 7- Color blending state
    // This is where we specify the color blending settings (e.g., blend enable, blend factors, etc.)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = params.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Dynamic states
    // This is where we specify the dynamic states (e.g., viewport, scissor, etc.)
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Graphics pipeline creation
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = params.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(_ctx->device, _ctx->pipelineCache, 1, &pipelineInfo, nullptr, &_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }else {
        spdlog::info("Graphics pipeline created successfully {}", _name != "" ? fmt::format("({})", _name) : "");
    }

    // Destroy shader modules after creating the pipeline
    vkDestroyShaderModule(_ctx->device, fragShaderModule, nullptr);
    vkDestroyShaderModule(_ctx->device, vertShaderModule, nullptr);
}


std::vector<char> Pipeline::readBinaryFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filename);
        return {};
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}