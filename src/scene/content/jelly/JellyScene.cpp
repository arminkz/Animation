#include "JellyScene.h"
#include "scene/camera/TurnTableCamera.h"
#include "core/AssetPath.h"
#include "gui/FontAwesome.h"
#include "vulkan/VulkanHelper.h"
#include "scene/physics/MassSpringSimulation.h"

JellyScene::JellyScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : SinglePassRenderer(std::move(ctx), std::move(swapChain))
{
    createSimulation();
    createScenePipelines();

    _jellyModel = std::make_shared<JellyModel>(_ctx, _sim.get());
    _jellyModel->setPipeline(_jellyPipeline);
    _sceneModels.push_back(_jellyModel);

    _sim->windEnabled    = false;
    _sim->gravityEnabled = true;
    _sim->stiffness      = 400.f;
    _sim->springDamping  = 3.f;
    _sim->damping        = 0.998f;
    _sim->substeps       = 16;

    TurnTableCameraParams cam{};
    cam.target           = glm::vec3(0.f, 3.f, 0.f);
    cam.initialRadius    = 20.f;
    cam.initialAzimuth   = glm::quarter_pi<float>();
    cam.initialElevation = glm::radians(-30.f);
    _camera = std::make_unique<TurnTableCamera>(cam);
}

JellyScene::~JellyScene()
{
    vkDeviceWaitIdle(_ctx->device);
    _jellyPipeline.reset();
}

void JellyScene::createSimulation()
{
    float spacing = 0.6f;
    int resX = 6, resY = 14, resZ = 6;
    float startY = _planeY + (resY - 1) * spacing * 0.5f + 4.f;
    _sim = std::make_unique<JellySimulation>(_ctx, resX, resY, resZ, spacing,
                                             glm::vec3(0.f, startY, 0.f));
}

void JellyScene::createScenePipelines()
{
    PipelineParams params;
    params.name                        = "JellyPipeline";
    params.vertexBindingDescription    = MSParticle::getBindingDescription();
    params.vertexAttributeDescriptions = MSParticle::getAttributeDescriptions();
    params.descriptorSetLayouts        = { _sceneDescriptorSets[0]->getDescriptorSetLayout() };
    params.pushConstantRanges          = { {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)} };
    params.renderPass                  = _mainRenderPass->getRenderPass();
    params.cullMode                    = VK_CULL_MODE_NONE;
    params.msaaSamples                 = _msaaSamples;

    _jellyPipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/jelly/jelly_vert.spv"),
        AssetPath::getInstance()->get("spv/jelly/jelly_frag.spv"),
        params);
}

void JellyScene::restart()
{
    vkDeviceWaitIdle(_ctx->device);

    auto savedStiffness    = _sim->stiffness;
    auto savedSpringDamping = _sim->springDamping;
    auto savedDamping      = _sim->damping;
    auto savedSubsteps     = _sim->substeps;
    auto savedGravity      = _sim->gravityEnabled;

    _sceneModels.erase(std::remove(_sceneModels.begin(), _sceneModels.end(), _jellyModel), _sceneModels.end());

    createSimulation();

    _sim->stiffness      = savedStiffness;
    _sim->springDamping  = savedSpringDamping;
    _sim->damping        = savedDamping;
    _sim->substeps       = savedSubsteps;
    _sim->gravityEnabled = savedGravity;
    _sim->windEnabled    = false;

    _jellyModel = std::make_shared<JellyModel>(_ctx, _sim.get());
    _jellyModel->setPipeline(_jellyPipeline);
    _sceneModels.push_back(_jellyModel);
}

void JellyScene::advance()
{
    auto now = std::chrono::high_resolution_clock::now();
    _dt = std::min(std::chrono::duration<float>(now - _lastFrameTime).count(), 1.f / 30.f);
    _lastFrameTime = now;

    if (_paused) _dt = 0.f;

    _sceneInfo.time += _dt;
    _camera->advanceAnimation(_dt);
}

void JellyScene::dispatchCompute(VkCommandBuffer cmd)
{
    if (_paused) return;

    MSCollider plane{};
    plane.type      = static_cast<int32_t>(MSColliderType::Plane);
    plane.position  = glm::vec3(0.f, _planeY, 0.f);
    plane.normal    = glm::vec3(0.f, 1.f, 0.f);
    plane.radius    = 0.f;
    plane.stiffness = _planeStiffness;
    plane.friction  = _planeFriction;
    _sim->setColliders({ plane });

    float subDt = _dt * _timeScale / static_cast<float>(_sim->substeps);
    for (int s = 0; s < _sim->substeps; ++s)
    {
        _sim->dispatch(cmd, subDt, _sceneInfo.time);
        if (s < _sim->substeps - 1)
            VulkanHelper::barrierComputeToCompute(cmd);
    }
    VulkanHelper::barrierComputeToVertex(cmd, _sim->getOutParticleBuffer());
}

void JellyScene::buildUI()
{
    ImGui::Begin("Jelly");
    ImGui::Text(ICON_FA_GAUGE " %.1f FPS  (%.2f ms)",
        ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    ImGui::Separator();
    ImGui::Text(ICON_FA_FLASK " Simulation");
    ImGui::Indent(16.0f);
    if (ImGui::Button(_paused ? ICON_FA_PLAY " Resume" : ICON_FA_PAUSE " Pause"))
        _paused = !_paused;
    ImGui::SameLine();
    if (ImGui::Button("Restart")) restart();
    ImGui::SliderFloat("Time Scale", &_timeScale, 0.1f, 5.f);
    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text(ICON_FA_WEIGHT_HANGING " Mass Spring");
    ImGui::Indent(16.0f);
    ImGui::SliderFloat("Stiffness",    &_sim->stiffness,     0.f,   3000.f);
    ImGui::SliderFloat("Spr. Damping", &_sim->springDamping, 0.f,   20.f);
    ImGui::SliderFloat("Vel. Damping", &_sim->damping,       0.9f,  1.f);
    ImGui::SliderInt  ("Substeps",     &_sim->substeps,      1,     32);
    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text(ICON_FA_SQUARE " Ground Plane");
    ImGui::Indent(16.0f);
    ImGui::SliderFloat("Plane Y",      &_planeY,            -5.f,   5.f);
    ImGui::SliderFloat("Stiffness##p", &_planeStiffness,     100.f, 20000.f);
    ImGui::SliderFloat("Friction##p",  &_planeFriction,      0.f,   2.f);
    ImGui::Unindent(16.0f);
    
    ImGui::End();
}
