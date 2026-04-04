#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/SwapChain.h"

class Renderer
{
public:
    Renderer(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
        : _ctx(std::move(ctx)), _swapChain(std::move(swapChain)) {}
    virtual ~Renderer() = default;
    
    // Update Logic
    virtual void update() {}; 

    // Compute Operations (Optional)
    virtual void dispatchCompute(VkCommandBuffer commandBuffer) {}

    // Graphics Operations
    virtual void recordToCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) = 0;
    virtual void onSwapChainRecreated() { }
    void setCurrentFrame(uint32_t frameIndex) { _currentFrame = frameIndex; } // (0 < currentFrame < MAX_FRAMES_IN_FLIGHT)

    // Handle mouse click events
    virtual void handleMouseClick(float mouseX, float mouseY) = 0;
    virtual void handleMouseDrag(float dx, float dy) = 0;
    virtual void handleMouseWheel(float dy) = 0;
    virtual void handleKeyDown(int key, int scancode, int modifiers) = 0;

    // Called each frame to build scene-specific ImGui controls
    virtual void buildUI() {};

protected:
    std::shared_ptr<VulkanContext> _ctx;
    std::shared_ptr<SwapChain> _swapChain;
    uint32_t _currentFrame = 0;
};
