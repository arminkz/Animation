#pragma once

#include "stdafx.h"
#include "core/SinglePassRenderer.h"
#include "scene/physics/JellySimulation.h"
#include "scene/content/jelly/JellyModel.h"

class JellyScene : public SinglePassRenderer
{
public:
    JellyScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~JellyScene();

    void advance() override;
    void dispatchCompute(VkCommandBuffer cmd) override;
    void buildUI() override;

private:
    void createSimulation();
    void createScenePipelines();
    void restart();

    std::unique_ptr<JellySimulation> _sim;
    std::shared_ptr<JellyModel>      _jellyModel;
    std::shared_ptr<Pipeline>        _jellyPipeline;

    float _planeY         = 0.f;
    float _planeStiffness = 10000.f;
    float _planeFriction  = 0.6f;

    float _timeScale = 2.5f;

    TimePoint _lastFrameTime = std::chrono::high_resolution_clock::now();
    float _dt = 0.f;
};
