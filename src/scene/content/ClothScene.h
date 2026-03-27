#pragma once

#include "stdafx.h"
#include "core/SinglePassRenderer.h"
#include "scene/content/ClothModel.h"


class ClothScene : public SinglePassRenderer
{
public:
    ClothScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~ClothScene();

    void advance() override final;
    void buildUI() override;

private:

    std::shared_ptr<Pipeline> _renderPipeline;
    std::shared_ptr<Pipeline> _wireframePipeline;
    bool _wireframe = false;
    void createScenePipelines();
    void connectPipelines();

    std::shared_ptr<ClothModel> _clothModel;
    void createModels();

    int _selectedTexture = 0;

    TimePoint _lastFrameTime = std::chrono::high_resolution_clock::now();
};
