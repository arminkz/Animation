#pragma once

#include "stdafx.h"
#include "core/SinglePassRenderer.h"
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
    void createJellyModel();
    void createScenePipelines();
    void restart();

    std::shared_ptr<JellyModel> _jellyModel;
    std::shared_ptr<Pipeline> _jellyPipeline;

    float _planeY         = 0.f;
    float _planeStiffness = 10000.f;
    float _planeFriction  = 0.6f;

    float _timeScale = 2.5f;

    // Fixed-timestep physics: every substep is always PHYSICS_DT long, so
    // the integrator's stored displacement `(pos - prevPos)` is always
    // accumulated over the same interval and frame-rate jitter can never
    // re-interpret it. `_physicsAccumulator` carries over any leftover
    // wall-clock time that didn't make a full step.
    static constexpr float PHYSICS_DT           = 1.f / 400.f; // 2.5 ms
    static constexpr int   MAX_STEPS_PER_FRAME  = 16;          // spiral-of-death guard
    float _physicsAccumulator = 0.f;

    TimePoint _lastFrameTime = std::chrono::high_resolution_clock::now();
    float _dt = 0.f;
    bool _paused = false;
};
