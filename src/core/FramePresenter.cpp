#include "core/FramePresenter.h"
#include "scene/content/flag/FlagScene.h"
#include "scene/content/table/TableScene.h"
#include "scene/content/jelly/JellyScene.h"
#include "scene/content/boid/BoidScene.h"

FramePresenter::FramePresenter(std::shared_ptr<VulkanContext> ctx)
    : _ctx(std::move(ctx))
{
    spdlog::info("Max Frames in flight: {}", MAX_FRAMES_IN_FLIGHT);

    _swapChain = std::make_shared<SwapChain>(_ctx);
    switchScene(_selectedScene);
    _gui = std::make_unique<GUI>(_ctx, _ctx->window, _swapChain->getSwapChainImageFormat());

    createCommandBuffers();
    createSyncObjects();
    createGUIFramebuffers();
}


FramePresenter::~FramePresenter()
{
    // Wait for any unfinished GPU tasks
    vkDeviceWaitIdle(_ctx->device);

    destroyGUIFramebuffers();
    destroySyncObjects();
}


void FramePresenter::createCommandBuffers() {
    // Allocate command buffer
    _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _ctx->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();

    if (vkAllocateCommandBuffers(_ctx->device, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
        spdlog::error("Failed to allocate command buffers!");
    } else {
        spdlog::info("Command buffers allocated successfully");
    }
}


void FramePresenter::createSyncObjects() {
    _imageAvailableSemaphores.resize(_swapChain->getSwapChainImageCount());
    _renderFinishedSemaphores.resize(_swapChain->getSwapChainImageCount());
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //Initially signaled

    for (size_t i = 0; i < _swapChain->getSwapChainImageCount(); i++) {
        if (vkCreateSemaphore(_ctx->device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_ctx->device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS) {
            spdlog::error("Failed to create semaphores for swap chain image {}!", i);
        }
    }

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(_ctx->device, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS) {
            spdlog::error("Failed to create fences for frame {}!", i);
        }
    }
}


void FramePresenter::destroySyncObjects() {
    for (auto sem : _imageAvailableSemaphores)
        vkDestroySemaphore(_ctx->device, sem, nullptr);
    for (auto sem : _renderFinishedSemaphores)
        vkDestroySemaphore(_ctx->device, sem, nullptr);
    for (auto fence : _inFlightFences)
        vkDestroyFence(_ctx->device, fence, nullptr);
    _imageAvailableSemaphores.clear();
    _renderFinishedSemaphores.clear();
    _inFlightFences.clear();
}


void FramePresenter::createGUIFramebuffers()
{
    const auto& imageViews = _swapChain->getSwapChainImageViews();
    VkExtent2D extent = _swapChain->getSwapChainExtent();

    _guiFramebuffers.resize(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = _gui->getRenderPass();
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &imageViews[i];
        fbInfo.width           = extent.width;
        fbInfo.height          = extent.height;
        fbInfo.layers          = 1;
        if (vkCreateFramebuffer(_ctx->device, &fbInfo, nullptr, &_guiFramebuffers[i]) != VK_SUCCESS) {
            spdlog::error("Failed to create GUI framebuffer!");
        }
    }

}


void FramePresenter::destroyGUIFramebuffers()
{
    for (auto fb : _guiFramebuffers)
        vkDestroyFramebuffer(_ctx->device, fb, nullptr);
    _guiFramebuffers.clear();
}


void FramePresenter::invalidate()
{
    vkDeviceWaitIdle(_ctx->device);
    spdlog::info("Recreating swapchain after window resize.");

    // Destroy semaphores/fences — their count depends on swapchain image count, which may change
    destroySyncObjects();

    destroyGUIFramebuffers();

    // Recreate swapchain with new window dimensions
    _swapChain->cleanupSwapChain();
    _swapChain->createSwapChain();

    createSyncObjects();
    createGUIFramebuffers();

    // Reset frame counters
    _frameCounter = 0;
    _imageCounter = 0;

    // Notify renderer to resize;
    _renderer->onSwapChainRecreated();
}


void FramePresenter::present() {
    // Wait for the previous frame to finish
    vkWaitForFences(_ctx->device, 1, &_inFlightFences[_frameCounter], VK_TRUE, UINT64_MAX);

    // Wait for a swap chain image to be available
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_ctx->device, _swapChain->getSwapChain(), UINT64_MAX, _imageAvailableSemaphores[_imageCounter], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        invalidate();
        return;
    } else if (result != VK_SUCCESS) {
        spdlog::error("Failed to acquire swap chain image!");
        return;
    }

    // Reset the fence before drawing
    vkResetFences(_ctx->device, 1, &_inFlightFences[_frameCounter]);
    vkResetCommandBuffer(_commandBuffers[_frameCounter], 0);

    // Build ImGui frame
    _gui->beginFrame();
    buildSceneSelector();
    _renderer->buildUI();

    // Update Scene
    _renderer->setCurrentFrame(_frameCounter);
    _renderer->update();

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(_commandBuffers[_frameCounter], &beginInfo) != VK_SUCCESS) {
        spdlog::error("Failed to begin recording command buffer!");
        return;
    }

    // Dispatch compute operations
    _renderer->dispatchCompute(_commandBuffers[_frameCounter]);

    // Record scene and GUI into the same command buffer, then end it
    _renderer->recordToCommandBuffer(_commandBuffers[_frameCounter], imageIndex);
    _gui->recordToCommandBuffer(_commandBuffers[_frameCounter], _guiFramebuffers[imageIndex], _swapChain->getSwapChainExtent());

    if (vkEndCommandBuffer(_commandBuffers[_frameCounter]) != VK_SUCCESS) {
        spdlog::error("Failed to end command buffer!");
        return;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[_imageCounter]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[_frameCounter];

    VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[_imageCounter]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(_ctx->graphicsQueue, 1, &submitInfo, _inFlightFences[_frameCounter]) != VK_SUCCESS) {
        spdlog::error("Failed to submit draw command buffer!");
        return;
    }

    // Present the image to the swap chain (after the command buffer is done)
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {_swapChain->getSwapChain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    result = vkQueuePresentKHR(_ctx->presentQueue, &presentInfo);
    
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
        invalidate();
    } else if (result != VK_SUCCESS) {
        spdlog::error("Failed to present swap chain image!");
    }

    _frameCounter = (_frameCounter + 1) % MAX_FRAMES_IN_FLIGHT;
    _imageCounter = (_imageCounter + 1) % _swapChain->getSwapChainImageCount();
}


void FramePresenter::buildSceneSelector()
{
    static const char* sceneNames[] = { "Flag", "Table", "Jelly", "Boid" };

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 60), ImGuiCond_Always);
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    int prev = _selectedScene;
    if (ImGui::Combo("Scene", &_selectedScene, sceneNames, IM_ARRAYSIZE(sceneNames)))
        if (_selectedScene != prev)
            switchScene(_selectedScene);
    ImGui::End();
}


void FramePresenter::switchScene(int index)
{
    vkDeviceWaitIdle(_ctx->device);
    spdlog::info("Switching scene...");
    switch (index) {
        case 0: _renderer = std::make_unique<FlagScene>(_ctx, _swapChain);  break;
        case 1: _renderer = std::make_unique<TableScene>(_ctx, _swapChain); break;
        case 2: _renderer = std::make_unique<JellyScene>(_ctx, _swapChain); break;
        case 3: _renderer = std::make_unique<BoidScene>(_ctx, _swapChain); break;
    }
}


void FramePresenter::handleEvent(SDL_Event* event) {
    // Always forward to ImGui so it can update its input state
    _gui->handleEvent(event);

    // Mouse Events — only forward to the scene if ImGui is not consuming the input
    if (!_gui->isCapturingMouse()) {
        switch (event->type) {
            case SDL_EVENT_MOUSE_MOTION:
                if (event->motion.state & SDL_BUTTON_LMASK)
                    _renderer->handleMouseDrag(event->motion.xrel, event->motion.yrel);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event->button.button == SDL_BUTTON_LEFT)
                    _renderer->handleMouseClick(event->button.x, event->button.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                _renderer->handleMouseWheel(event->wheel.y);
                break;
            default:
                break;
        }
    }

    // Keyboard Events — only forward to the scene if ImGui is not consuming the input
    if (!_gui->isCapturingKeyboard()) {
        switch (event->type) {
            case SDL_EVENT_KEY_DOWN:
                _renderer->handleKeyDown(event->key.key, event->key.scancode, event->key.mod);
                break;
            default:
                break;
        }
    }
}