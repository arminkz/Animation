#include "JellyScene.h"
#include "scene/camera/TurnTableCamera.h"
#include "core/AssetPath.h"
#include "gui/FontAwesome.h"
#include "vulkan/VulkanHelper.h"
#include "scene/physics/MassSpringSimulation.h"

JellyScene::JellyScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : SinglePassRenderer(std::move(ctx), std::move(swapChain))
{
    createScenePipelines();
    createJellyModel();
    _jellyModel->setPipeline(_jellyPipeline);
    _sceneModels.push_back(_jellyModel);

    auto* sim = _jellyModel->getSimulation();
    sim->windEnabled    = false;
    sim->gravityEnabled = true;
    sim->stiffness      = 400.f;
    sim->springDamping  = 3.f;
    sim->velocityDamping        = 0.998f;

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

void JellyScene::createJellyModel()
{
    float spacing = 0.6f;
    int resX = 6, resY = 14, resZ = 6;
    float startY = _planeY + (resY - 1) * spacing * 0.5f + 4.f;
    _jellyModel = std::make_shared<JellyModel>(_ctx, resX, resY, resZ, spacing,
                                               glm::vec3(0.f, startY, 0.f));
}

void JellyScene::createScenePipelines()
{
    PipelineParams params;
    params.name                        = "JellyPipeline";
    params.vertexBindingDescription    = JellyVertex::getBindingDescription();
    params.vertexAttributeDescriptions = JellyVertex::getAttributeDescriptions();
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

    auto* sim = _jellyModel->getSimulation();
    auto savedStiffness     = sim->stiffness;
    auto savedSpringDamping = sim->springDamping;
    auto savedDamping       = sim->velocityDamping;
    auto savedGravity       = sim->gravityEnabled;

    _sceneModels.erase(std::remove(_sceneModels.begin(), _sceneModels.end(), _jellyModel), _sceneModels.end());

    createJellyModel();
    sim = _jellyModel->getSimulation();
    sim->stiffness      = savedStiffness;
    sim->springDamping  = savedSpringDamping;
    sim->velocityDamping        = savedDamping;
    sim->gravityEnabled = savedGravity;
    sim->windEnabled    = false;

    _physicsAccumulator = 0.f;

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

    auto* sim = _jellyModel->getSimulation();

    MSCollider plane{};
    plane.type      = static_cast<int32_t>(MSColliderType::Plane);
    plane.position  = glm::vec3(0.f, _planeY, 0.f);
    plane.normal    = glm::vec3(0.f, 1.f, 0.f);
    plane.radius    = 0.f;
    plane.stiffness = _planeStiffness;
    plane.friction  = _planeFriction;
    sim->setColliders({ plane });

    // Fixed-timestep accumulator. Every substep runs at exactly PHYSICS_DT,
    // so `(pos - prevPos)` is always accumulated over a constant interval
    // and frame-rate jitter cannot kick the sim out of equilibrium.
    _physicsAccumulator += _dt * _timeScale;

    int steps = 0;
    while (_physicsAccumulator >= PHYSICS_DT && steps < MAX_STEPS_PER_FRAME)
    {
        sim->dispatch(cmd, PHYSICS_DT, _sceneInfo.time);
        _physicsAccumulator -= PHYSICS_DT;
        ++steps;

        // Barrier between consecutive physics substeps only.
        if (_physicsAccumulator >= PHYSICS_DT && steps < MAX_STEPS_PER_FRAME)
            VulkanHelper::barrierComputeToCompute(cmd);
    }

    // If we hit the per-frame cap, drop any leftover accumulated time —
    // otherwise a hitch would let the sim "catch up" and visibly snap.
    if (steps == MAX_STEPS_PER_FRAME)
        _physicsAccumulator = 0.f;

    // Only re-run the render-normals compute pass if physics actually stepped
    // this frame. Otherwise the previous frame's render vertex buffer is
    // still valid and can be bound as-is.
    if (steps > 0)
    {
        VulkanHelper::barrierComputeToCompute(cmd);
        sim->dispatchPreRender(cmd);
        VulkanHelper::barrierComputeToVertex(cmd, sim->getVertexBuffer());
    }
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

    auto* sim = _jellyModel->getSimulation();

    ImGui::Separator();
    ImGui::Text(ICON_FA_WEIGHT_HANGING " Mass Spring");
    ImGui::Indent(16.0f);
    ImGui::SliderFloat("Stiffness",    &sim->stiffness,     0.f,   3000.f);
    ImGui::SliderFloat("Spr. Damping", &sim->springDamping, 0.f,   20.f);
    ImGui::SliderFloat("Vel. Damping", &sim->velocityDamping,       0.9f,  1.f);
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
