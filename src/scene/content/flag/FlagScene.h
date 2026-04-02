#pragma once

#include "stdafx.h"
#include "core/SinglePassRenderer.h"
#include "scene/content/flag/ClothModel.h"


class FlagScene : public SinglePassRenderer
{
public:
    FlagScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~FlagScene();

    void advance() override final;
    void dispatchCompute(VkCommandBuffer commandBuffer) override; // this scene uses compute shaders
    void buildUI() override;

private:

    std::shared_ptr<Pipeline> _renderPipeline;
    std::shared_ptr<Pipeline> _wireframePipeline;
    void createScenePipelines();
    void connectPipelines();

    std::shared_ptr<ClothModel> _clothModel;
    void createModels();

    void createCamera();

    TimePoint _lastFrameTime = std::chrono::high_resolution_clock::now();
    float _dt = 0.f;    
    bool _wireframe = false;
    int _selectedTexture = 0;
};
