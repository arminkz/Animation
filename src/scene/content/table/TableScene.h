#pragma once

#include "stdafx.h"
#include "core/SinglePassRenderer.h"
#include "scene/content/flag/ClothModel.h"
#include "scene/content/table/SphereModel.h"

class TableScene : public SinglePassRenderer
{
public:
    TableScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~TableScene();

    void advance() override;
    void dispatchCompute(VkCommandBuffer cmd) override;
    void buildUI() override;

private:
    std::shared_ptr<Pipeline> _clothRenderPipeline;
    std::shared_ptr<Pipeline> _clothWireframePipeline;
    std::shared_ptr<Pipeline> _spherePipeline;
    void createScenePipelines();

    std::shared_ptr<ClothModel>  _clothModel;
    std::shared_ptr<SphereModel> _sphereModel;
    void createModels();
    void restartCloth();

    // Sphere collider params
    glm::vec3 _sphereCenter  = glm::vec3(0.f, 0.f, 0.f);
    float     _sphereRadius  = 4.f;
    float     _colliderOffset    = 0.05f;
    float     _colliderStiffness = 8000.f;
    float     _colliderFriction  = 0.5f;

    float _timeScale = 2.f;
    float _dropOffsetX = 0.f;

    TimePoint _lastFrameTime = std::chrono::high_resolution_clock::now();
    float _dt = 0.f;
    bool _wireframe = false;
};
