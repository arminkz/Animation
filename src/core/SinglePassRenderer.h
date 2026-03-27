#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/Pipeline.h"
#include "vulkan/FrameBuffer.h"
#include "vulkan/RenderPass.h"
#include "vulkan/resources/Buffer.h"
#include "vulkan/DescriptorSet.h"
#include "core/Renderer.h"
#include "scene/Camera.h"
#include "scene/SelectableModel.h"
#include "scene/Model.h"


class SinglePassRenderer : public Renderer
{
public:
    SinglePassRenderer(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~SinglePassRenderer();

    void update() override final;
    virtual void advance() = 0;
    void recordToCommandBuffer(VkCommandBuffer commandBuffer, uint32_t targetSwapImageIndex) override;
    void onSwapChainRecreated() override;

    void handleMouseClick(float mouseX, float mouseY) override;
    void handleMouseDrag(float dx, float dy) override;
    void handleMouseWheel(float dy) override;
    void handleKeyDown(int key, int scancode, int modifiers) override;

    const DescriptorSet* getSceneDescriptorSet() const { return _sceneDescriptorSets[_currentFrame].get(); }

protected:
    // Scene UBO
    struct SceneInfo {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;
        alignas(4)  float time = 0.0f;
        alignas(16) glm::vec3 cameraPosition;
        alignas(16) glm::vec3 lightColor;
    } _sceneInfo;
    
    std::array<std::unique_ptr<Buffer>, MAX_FRAMES_IN_FLIGHT> _sceneInfoUBOs;
    std::array<std::unique_ptr<DescriptorSet>, MAX_FRAMES_IN_FLIGHT> _sceneDescriptorSets;

    // Scene Models
    std::vector<std::shared_ptr<Model>> _sceneModels;
    std::unordered_map<int, std::shared_ptr<SelectableModel>> _selectableModels;
    
    // Camera
    std::unique_ptr<Camera> _camera = nullptr;
    uint32_t _currentTargetObjectID = 0;

    // MSAA
    VkSampleCountFlagBits _msaaSamples;

    // Scene (main) render pass
    std::unique_ptr<RenderPass> _mainRenderPass;
    

private:
    // Render passes
    void createRenderPass();

    // Framebuffers
    std::vector<std::unique_ptr<FrameBuffer>> _mainFrameBuffers;
    void createFrameBuffers();

    // Object selection
    struct ObjectSelectionPushConstants {
        alignas(16) glm::mat4 model;
        alignas(4)  uint32_t objectID;
    };

    std::unique_ptr<RenderPass> _objectSelectionRenderPass;
    std::unique_ptr<FrameBuffer> _objectSelectionFrameBuffer;
    std::unique_ptr<Pipeline> _objectSelectionPipeline;

    void createObjectSelectionPipeline();
    uint32_t querySelectionImage(float mouseX, float mouseY);
};