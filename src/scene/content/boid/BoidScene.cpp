#include "BoidScene.h"

#include "core/AssetPath.h"
#include "gui/FontAwesome.h"
#include "scene/camera/TurnTableCamera.h"
#include "vulkan/VulkanHelper.h"

BoidScene::BoidScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : SinglePassRenderer(std::move(ctx), std::move(swapChain))
{
    createModels();
    createScenePipelines();
    connectPipelines();
    createCamera();
}


void BoidScene::createModels()
{
    _skyboxModel = std::make_shared<SkyBox>(_ctx, "textures/aqua_skybox");
    _sceneModels.push_back(_skyboxModel);

    _agentModel = std::make_shared<ArrowModel>(_ctx);
    _sceneModels.push_back(_agentModel);

    resetBoids();
}


void BoidScene::createScenePipelines()
{
    // Boid arrow pipeline — uses boid_vert.spv (instanced, reads transforms from SSBO at set=1)
    PipelineParams params;
    params.name                        = "BoidPipeline";
    params.vertexBindingDescription    = Vertex::getBindingDescription();
    params.vertexAttributeDescriptions = Vertex::getAttributeDescriptions();
    params.descriptorSetLayouts        = {
        _sceneDescriptorSets[0]->getDescriptorSetLayout(),                 // set=0: SceneInfo
        _agentModel->getTransformDescriptorSet()->getDescriptorSetLayout() // set=1: transforms SSBO
    };
    params.pushConstantRanges          = {};   // no push constants — model comes from SSBO
    params.renderPass                  = _mainRenderPass->getRenderPass();
    params.cullMode                    = VK_CULL_MODE_BACK_BIT;
    params.msaaSamples                 = _msaaSamples;
    params.blendEnable                 = false;
    _renderPipeline = std::make_shared<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/boid/boid_vert.spv"),
        AssetPath::getInstance()->get("spv/shader_frag.spv"),
        params);

    // Skybox pipeline (unchanged)
    PipelineParams skyParams;
    skyParams.name                        = "SkyBoxPipeline";
    skyParams.vertexBindingDescription    = Vertex::getBindingDescription();
    skyParams.vertexAttributeDescriptions = Vertex::getAttributeDescriptions();
    skyParams.descriptorSetLayouts        = {
        _sceneDescriptorSets[0]->getDescriptorSetLayout(),
        _skyboxModel->getDescriptorSet()->getDescriptorSetLayout()
    };
    skyParams.pushConstantRanges          = { {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)} };
    skyParams.renderPass                  = _mainRenderPass->getRenderPass();
    skyParams.cullMode                    = VK_CULL_MODE_FRONT_BIT;
    skyParams.msaaSamples                 = _msaaSamples;
    skyParams.depthWrite                  = false;
    skyParams.depthCompareOp              = VK_COMPARE_OP_LESS_OR_EQUAL;
    _skyboxPipeline = std::make_shared<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/skybox/skybox_vert.spv"),
        AssetPath::getInstance()->get("spv/skybox/skybox_frag.spv"),
        skyParams);
}


void BoidScene::connectPipelines()
{
    _skyboxModel->setPipeline(_skyboxPipeline);
    _agentModel->setPipeline(_renderPipeline);
}


void BoidScene::advance()
{
    auto now = std::chrono::high_resolution_clock::now();
    _dt = std::chrono::duration<float>(now - _lastFrameTime).count();
    _lastFrameTime = now;

    _camera->advanceAnimation(_dt);
}


void BoidScene::dispatchCompute(VkCommandBuffer cmd)
{
    _boidSim->dispatch(cmd, _dt * _timeScale);
    VulkanHelper::barrierComputeToVertex(cmd, _boidSim->getTransformBuffer());
}


void BoidScene::resetBoids()
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-SPAWN_RANGE, SPAWN_RANGE);
    std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);

    std::vector<BoidState> states;
    states.reserve(_nBoids);

    while ((int)states.size() < _nBoids) {
        float x = unitDist(rng), y = unitDist(rng), z = unitDist(rng);
        float len = std::sqrt(x*x + y*y + z*z);
        if (len < 1e-4f || len > 1.0f) continue;

        BoidState b{};
        float initSpeed = _boidSim ? _boidSim->minSpeed : 8.f;
        b.position = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
        b.velocity = glm::vec3(x, y, z) / len * initSpeed;
        states.push_back(b);
    }

    if (_boidSim) {
        _boidSim->resetBoids(states);
    } else {
        _boidSim = std::make_unique<BoidSimulation>(_ctx, _nBoids, states, SPAWN_RANGE);
        _agentModel->setTransformBuffer(_boidSim->getTransformBuffer(), _nBoids);
    }
}


void BoidScene::buildUI()
{
    ImGui::Begin("Boids");

    if (ImGui::SliderInt("Number of Boids", &_nBoids, 200, 2000)) {
        _boidSim.reset();
        resetBoids();
        // Rebuild pipeline since transform DS layout is recreated
        createScenePipelines();
        connectPipelines();
    }

    ImGui::SeparatorText("Separation");
    ImGui::SliderFloat("rs (separation radius)", &_boidSim->rs, 1.f, 20.f);
    ImGui::SliderFloat("ws (separation weight)", &_boidSim->ws, 0.f,  5.f);

    ImGui::SeparatorText("Alignment");
    ImGui::SliderFloat("ra (alignment radius)",  &_boidSim->ra, 1.f, 50.f);
    ImGui::SliderFloat("wa (alignment weight)",  &_boidSim->wa, 0.f,  5.f);

    ImGui::SeparatorText("Cohesion");
    ImGui::SliderFloat("rc (cohesion radius)",   &_boidSim->rc, 1.f, 50.f);
    ImGui::SliderFloat("wc (cohesion weight)",   &_boidSim->wc, 0.f,  5.f);

    ImGui::SeparatorText("Speed");
    ImGui::SliderFloat("Min Speed", &_boidSim->minSpeed, 0.f, _boidSim->maxSpeed);
    ImGui::SliderFloat("Max Speed", &_boidSim->maxSpeed, _boidSim->minSpeed, 50.f);
    ImGui::SliderFloat("Time Scale", &_timeScale, 0.f, 3.f);

    ImGui::End();
}


void BoidScene::createCamera()
{
    TurnTableCameraParams cam{};
    cam.target           = glm::vec3(0.f);
    cam.initialRadius    = 60.f;
    cam.initialAzimuth   = 0.f;
    cam.initialElevation = glm::radians(-60.f);
    _camera = std::make_unique<TurnTableCamera>(cam);
}
