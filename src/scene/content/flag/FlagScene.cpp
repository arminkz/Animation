#include "FlagScene.h"
#include "scene/camera/TurnTableCamera.h"

#include "gui/FontAwesome.h"
#include "core/AssetPath.h"
#include "geometry/Vertex.h"
#include "vulkan/VulkanHelper.h"

FlagScene::FlagScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : SinglePassRenderer(std::move(ctx), std::move(swapChain))
{
    createModels();
    createScenePipelines();
    connectPipelines();
    createCamera();
}


FlagScene::~FlagScene()
{
    // TODO: we can remove this part since we dont care in which order they get destroyed.
    vkDeviceWaitIdle(_ctx->device);
    _renderPipeline.reset();
    _wireframePipeline.reset();
}


void FlagScene::createModels()
{
    _clothModel = std::make_shared<ClothModel>(_ctx, 21, 33, 0.5f); // 32/20 = 1.6 aspect ratio
    _clothModel->setTexture(AssetPath::getInstance()->get("textures/flag_iran.png"));
    _sceneModels.push_back(_clothModel);
}


void FlagScene::createScenePipelines()
{
    PipelineParams params;
    params.name = "ClothPipeline";
    params.vertexBindingDescription = MSParticle::getBindingDescription();
    params.vertexAttributeDescriptions = MSParticle::getAttributeDescriptions();
    params.descriptorSetLayouts = {
        _sceneDescriptorSets[0]->getDescriptorSetLayout(),
        _clothModel->getDescriptorSet()->getDescriptorSetLayout()
    };
    params.pushConstantRanges = { {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)} };
    params.renderPass = _mainRenderPass->getRenderPass();
    params.cullMode = VK_CULL_MODE_NONE; // cloth is visible from both sides
    params.msaaSamples = _msaaSamples;

    _renderPipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/cloth/cloth_vert.spv"),
        AssetPath::getInstance()->get("spv/cloth/cloth_frag.spv"),
        params);

    params.name = "ClothWireframePipeline";
    params.polygonMode = VK_POLYGON_MODE_LINE;
    _wireframePipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/cloth/cloth_vert.spv"),
        AssetPath::getInstance()->get("spv/cloth/cloth_frag.spv"),
        params);
}


void FlagScene::connectPipelines()
{
    _clothModel->setPipeline(_renderPipeline);
}


void FlagScene::createCamera() {
    TurnTableCameraParams cameraParams{};
    cameraParams.target          = glm::vec3(0.f);
    cameraParams.initialRadius   = glm::length(glm::vec3(0.f, 30.f, 30.f));
    cameraParams.initialAzimuth  = glm::half_pi<float>();
    cameraParams.initialElevation = 0.f; // -glm::quarter_pi<float>(); // 45° below horizon
    _camera = std::make_unique<TurnTableCamera>(cameraParams);
}


void FlagScene::advance()
{
    // advance time
    auto now = std::chrono::high_resolution_clock::now();
    _dt = std::chrono::duration<float>(now - _lastFrameTime).count();
    _lastFrameTime = now;

    // advance time in UBO
    _sceneInfo.time += _dt;

    // advance camera
    _camera->advanceAnimation(_dt);
}


void FlagScene::dispatchCompute(VkCommandBuffer cmd)
{
    ClothSimulation* sim = _clothModel->getSimulation();

    float subDt = _dt / static_cast<float>(sim->substeps);

    for (int s = 0; s < sim->substeps; ++s)
    {
        sim->dispatch(cmd, subDt, _sceneInfo.time);

        if (s < sim->substeps - 1)
            VulkanHelper::barrierComputeToCompute(cmd);
    }

    VulkanHelper::barrierComputeToVertex(cmd, sim->getOutParticleBuffer());
}


void FlagScene::buildUI()
{
    ImGui::Begin("Flag");
    ImGui::Text(ICON_FA_GAUGE " %.1f FPS  (%.2f ms)",
        ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    ImGui::Separator();
    ImGui::Text(ICON_FA_FLAG  " Flag");
    ImGui::Indent(16.0f);
    static const char* textureNames[] = { "Iranian Flag", "US Flag", "Israeli Flag"};
    static const char* texturePaths[] = {
        "textures/flag_iran.png",
        "textures/flag_us.png",
        "textures/flag_israel.png"
    };
    if (ImGui::Combo("Texture", &_selectedTexture, textureNames, IM_ARRAYSIZE(textureNames)))
        _clothModel->setTexture(AssetPath::getInstance()->get(texturePaths[_selectedTexture]));

    if (ImGui::Checkbox("Wireframe", &_wireframe))
        _clothModel->setPipeline(_wireframe ? _wireframePipeline : _renderPipeline);
    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text(ICON_FA_WEIGHT_HANGING " Mass Spring");
    ImGui::Indent(16.0f);
    auto& sim = *_clothModel->getSimulation();
    ImGui::SliderFloat("Stiffness",       &sim.stiffness,     0.f, 2000.f);
    ImGui::SliderFloat("Spring Damping",  &sim.springDamping, 0.f, 20.f);
    ImGui::SliderFloat("Velocity Damping",&sim.damping,       0.f, 1.f);
    ImGui::SliderInt  ("Substeps",        &sim.substeps,      1,   20);
    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text(ICON_FA_WIND " Forces");
    ImGui::Indent(16.0f);
    ImGui::Checkbox("Gravity", &sim.gravityEnabled);
    ImGui::Checkbox("Wind",    &sim.windEnabled);
    if (sim.windEnabled)
    {
        ImGui::SliderFloat("Wind Strength",   &sim.windStrength,   0.f, 50.f);
        ImGui::SliderFloat("Wind Drag (Cd)",  &sim.windDragCoeff,  0.f, 10.f);
        ImGui::SliderFloat("Wind Turbulence", &sim.windTurbulence, 0.f, 20.f);
        ImGui::DragFloat3 ("Wind Direction",  &sim.windDirection.x, 0.01f, -1.f, 1.f);
    }
    ImGui::Unindent(16.0f);
    

    ImGui::End();
}

