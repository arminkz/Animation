#pragma once

#include "stdafx.h"
#include "core/SinglePassRenderer.h"
#include "scene/SkyBox.h"
#include "scene/physics/BoidSimulation.h"
#include "ArrowModel.h"

class BoidScene : public SinglePassRenderer
{
public:
    BoidScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);

    void advance()  override;
    void buildUI()  override;

    void dispatchCompute(VkCommandBuffer cmd) override;

private:
    std::shared_ptr<Pipeline> _renderPipeline;
    std::shared_ptr<Pipeline> _skyboxPipeline;
    void createScenePipelines();
    void connectPipelines();

    std::shared_ptr<SkyBox>      _skyboxModel;
    std::shared_ptr<ArrowModel>  _agentModel;
    std::unique_ptr<BoidSimulation> _boidSim;

    void createModels();
    void createCamera();
    void resetBoids();

    // Time
    float _timeScale = 1.f;
    TimePoint _lastFrameTime = std::chrono::high_resolution_clock::now();
    float _dt = 0.f;

    int _nBoids = 500;

    // Spawn range (must match SPAWN_RANGE constant in .cpp)
    static constexpr float SPAWN_RANGE = 100.f;
};
