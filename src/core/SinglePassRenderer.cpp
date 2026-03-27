#include "SinglePassRenderer.h"

#include "vulkan/VulkanHelper.h"
#include "core/AssetPath.h"


SinglePassRenderer::SinglePassRenderer(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : Renderer(std::move(ctx), std::move(swapChain))
{
    _msaaSamples = VulkanHelper::getMaxMsaaSampleCount(_ctx);

    _sceneInfo.view           = glm::mat4(1.0f);
    _sceneInfo.projection     = glm::mat4(1.0f);
    _sceneInfo.time           = 0.0f;
    _sceneInfo.cameraPosition = glm::vec3(0.0f);
    _sceneInfo.lightColor     = glm::vec3(1.0f);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _sceneInfoUBOs[i] = std::make_unique<Buffer>(_ctx, sizeof(SceneInfo),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        _sceneDescriptorSets[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                1, _sceneInfoUBOs[i]->getDescriptorInfo())
        });
    }

    createRenderPass();
    createFrameBuffers();
    createObjectSelectionPipeline();
}


SinglePassRenderer::~SinglePassRenderer()
{
    vkDeviceWaitIdle(_ctx->device);

    _mainRenderPass           = nullptr;
    _objectSelectionRenderPass = nullptr;
}


void SinglePassRenderer::createRenderPass()
{
    RenderPassParams sceneParams;
    sceneParams.name                    = "Scene Renderpass";
    sceneParams.colorFormat             = _swapChain->getSwapChainImageFormat();
    sceneParams.depthFormat             = VulkanHelper::findDepthFormat(_ctx);
    sceneParams.resolveFormat           = _swapChain->getSwapChainImageFormat();
    sceneParams.colorAttachmentLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    sceneParams.depthAttachmentLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    sceneParams.resolveAttachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    sceneParams.useColor                = true;
    sceneParams.useDepth                = true;
    sceneParams.useResolve              = true;
    sceneParams.msaaSamples             = _msaaSamples;
    sceneParams.isMultiPass             = false;
    _mainRenderPass = std::make_unique<RenderPass>(_ctx, sceneParams);

    RenderPassParams selectionParams;
    selectionParams.name                  = "Object Selection Renderpass";
    selectionParams.colorFormat           = VK_FORMAT_R32_UINT;
    selectionParams.depthFormat           = VulkanHelper::findDepthFormat(_ctx);
    selectionParams.colorAttachmentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    selectionParams.depthAttachmentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    selectionParams.useColor              = true;
    selectionParams.useDepth              = true;
    selectionParams.useResolve            = false;
    selectionParams.msaaSamples           = VK_SAMPLE_COUNT_1_BIT;
    selectionParams.isMultiPass           = false;
    _objectSelectionRenderPass = std::make_unique<RenderPass>(_ctx, selectionParams);
}


void SinglePassRenderer::createFrameBuffers()
{
    FrameBufferParams mainParams{};
    mainParams.extent       = _swapChain->getSwapChainExtent();
    mainParams.renderPass   = _mainRenderPass->getRenderPass();
    mainParams.msaaSamples  = _msaaSamples;
    mainParams.hasColor     = true;
    mainParams.hasDepth     = true;
    mainParams.hasResolve   = true;
    mainParams.colorFormat  = _swapChain->getSwapChainImageFormat();
    mainParams.depthFormat  = VulkanHelper::findDepthFormat(_ctx);
    mainParams.colorUsage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    mainParams.depthUsage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    _mainFrameBuffers.resize(_swapChain->getSwapChainImageViews().size());
    for (size_t i = 0; i < _swapChain->getSwapChainImageViews().size(); i++) {
        mainParams.resolveImageView = _swapChain->getSwapChainImageViews()[i];
        _mainFrameBuffers[i] = std::make_unique<FrameBuffer>(_ctx, mainParams);
    }

    FrameBufferParams selectionParams{};
    selectionParams.extent       = _swapChain->getSwapChainExtent();
    selectionParams.renderPass   = _objectSelectionRenderPass->getRenderPass();
    selectionParams.msaaSamples  = VK_SAMPLE_COUNT_1_BIT;
    selectionParams.hasColor     = true;
    selectionParams.hasDepth     = true;
    selectionParams.hasResolve   = false;
    selectionParams.colorFormat  = VK_FORMAT_R32_UINT;
    selectionParams.depthFormat  = VulkanHelper::findDepthFormat(_ctx);
    selectionParams.colorUsage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    selectionParams.depthUsage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    _objectSelectionFrameBuffer = std::make_unique<FrameBuffer>(_ctx, selectionParams);
}


void SinglePassRenderer::createObjectSelectionPipeline()
{
    VkDescriptorSetLayout sceneDSL = _sceneDescriptorSets[0]->getDescriptorSetLayout();

    PipelineParams selectionParams;
    selectionParams.name                 = "ObjectSelectionPipeline";
    selectionParams.descriptorSetLayouts = {sceneDSL};
    selectionParams.pushConstantRanges   = {{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ObjectSelectionPushConstants)}};
    selectionParams.renderPass           = _objectSelectionRenderPass->getRenderPass();
    selectionParams.blendEnable          = false;
    _objectSelectionPipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/selection/select_vert.spv"),
        AssetPath::getInstance()->get("spv/selection/select_frag.spv"),
        selectionParams);
}


void SinglePassRenderer::update()
{
    VkExtent2D extent = _swapChain->getSwapChainExtent();

    // Renderer specific updates (View, Projection, SceneUBO)
    _sceneInfo.view       = _camera->getViewMatrix();
    _sceneInfo.projection = glm::perspective(glm::radians(45.f),(float)extent.width / (float)extent.height, 0.1f, 4000.f);
    _sceneInfo.projection[1][1] *= -1;
    _sceneInfo.cameraPosition = _camera->getPosition();

    // Application specific updates (present in child class)
    advance();

    // Flush Scene UBO
    _sceneInfoUBOs[_currentFrame]->copyData(&_sceneInfo, sizeof(_sceneInfo));
}


void SinglePassRenderer::recordToCommandBuffer(VkCommandBuffer commandBuffer, uint32_t targetSwapImageIndex)
{
    //when we reach here command buffer is already began. just record draw commands
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil     = {1.0f, 0};

    VkRenderPassBeginInfo sceneInfo{};
    sceneInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    sceneInfo.renderPass        = _mainRenderPass->getRenderPass();
    sceneInfo.framebuffer       = _mainFrameBuffers[targetSwapImageIndex]->getFrameBuffer();
    sceneInfo.renderArea.offset = {0, 0};
    sceneInfo.renderArea.extent = _swapChain->getSwapChainExtent();
    sceneInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    sceneInfo.pClearValues      = clearValues.data();
    vkCmdBeginRenderPass(commandBuffer, &sceneInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.width    = static_cast<float>(_swapChain->getSwapChainExtent().width);
    vp.height   = static_cast<float>(_swapChain->getSwapChainExtent().height);
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &vp);

    VkRect2D sc{};
    sc.extent = _swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &sc);

    for (auto& model : _sceneModels)
        model->draw(commandBuffer, *this);

    vkCmdEndRenderPass(commandBuffer);
}


void SinglePassRenderer::onSwapChainRecreated()
{
    vkDeviceWaitIdle(_ctx->device);
    _mainFrameBuffers.clear();
    _objectSelectionFrameBuffer.reset();
    createFrameBuffers();
}


void SinglePassRenderer::handleMouseClick(float mouseX, float mouseY)
{
    uint32_t objectID = querySelectionImage(mouseX, mouseY);
    if (_currentTargetObjectID == objectID) return;

    //TODO: this is application specific and should not be here
    if (_selectableModels.find(objectID) != _selectableModels.end()) {
        _currentTargetObjectID = objectID;
        _camera->setTargetAnimated(_selectableModels[objectID]->getPosition());
    }
}


void SinglePassRenderer::handleMouseDrag(float dx, float dy)
{
    _camera->rotateHorizontally(static_cast<float>(dx) * 0.005f);
    _camera->rotateVertically(static_cast<float>(dy) * 0.005f);
}


void SinglePassRenderer::handleMouseWheel(float dy)
{
    float zoomDelta = dy * _camera->getRadius() * 0.03f;
    _camera->changeZoom(zoomDelta);
}


void SinglePassRenderer::handleKeyDown(int key, int scancode, int modifiers)
{
    
}


uint32_t SinglePassRenderer::querySelectionImage(float mouseX, float mouseY)
{
    VkCommandBuffer cmdBuffer = VulkanHelper::beginSingleTimeCommands(_ctx);

    VkRenderPassBeginInfo info{};
    info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass        = _objectSelectionRenderPass->getRenderPass();
    info.framebuffer       = _objectSelectionFrameBuffer->getFrameBuffer();
    info.renderArea.offset = {0, 0};
    info.renderArea.extent = _objectSelectionFrameBuffer->getExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color.uint32[0] = 0;
    clearValues[1].depthStencil    = {1.0f, 0};
    info.clearValueCount = static_cast<uint32_t>(clearValues.size());
    info.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.width    = static_cast<float>(_objectSelectionFrameBuffer->getExtent().width);
    vp.height   = static_cast<float>(_objectSelectionFrameBuffer->getExtent().height);
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {static_cast<int32_t>(mouseX), static_cast<int32_t>(mouseY)};
    scissor.extent = {1, 1};
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    _objectSelectionPipeline->bind(cmdBuffer);

    for (const auto& pair : _selectableModels) {
        VkBuffer vertexBuffers[] = {pair.second->getDeviceMesh()->getVertexBuffer()};
        VkDeviceSize offsets[]   = {0};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, pair.second->getDeviceMesh()->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        std::array<VkDescriptorSet, 1> descriptorSets = {
            _sceneDescriptorSets[_currentFrame]->getDescriptorSet()
        };
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            _objectSelectionPipeline->getPipelineLayout(), 0, 1, descriptorSets.data(), 0, nullptr);

        ObjectSelectionPushConstants pushConstants{pair.second->getModelMatrix(), static_cast<uint32_t>(pair.second->getID())};
        vkCmdPushConstants(cmdBuffer, _objectSelectionPipeline->getPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ObjectSelectionPushConstants), &pushConstants);

        vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(pair.second->getDeviceMesh()->getIndicesCount()), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmdBuffer);
    VulkanHelper::endSingleTimeCommands(_ctx, cmdBuffer);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkDeviceSize imageSize = _swapChain->getSwapChainExtent().width *
                             _swapChain->getSwapChainExtent().height * sizeof(uint32_t);
    VulkanHelper::createBuffer(_ctx, imageSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false, stagingBuffer, stagingMemory);

    VulkanHelper::copyImageToBuffer(_ctx, _objectSelectionFrameBuffer->getColorImage(),
        stagingBuffer, _swapChain->getSwapChainExtent().width, _swapChain->getSwapChainExtent().height);

    uint32_t* pixelData = new uint32_t[_swapChain->getSwapChainExtent().width * _swapChain->getSwapChainExtent().height];
    void* data;
    vkMapMemory(_ctx->device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(pixelData, data, static_cast<size_t>(imageSize));
    vkUnmapMemory(_ctx->device, stagingMemory);
    vkDestroyBuffer(_ctx->device, stagingBuffer, nullptr);
    vkFreeMemory(_ctx->device, stagingMemory, nullptr);

    int mousePixel = static_cast<int>(mouseX) + static_cast<int>(mouseY) * _swapChain->getSwapChainExtent().width;
    uint32_t selectedID = pixelData[mousePixel];
    delete[] pixelData;
    return selectedID;
}
