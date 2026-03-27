#include "ClothScene.h"
#include "scene/TurnTableCamera.h"

#include "gui/FontAwesome.h"
#include "core/AssetPath.h"
#include "geometry/Vertex.h"

ClothScene::ClothScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : SinglePassRenderer(std::move(ctx), std::move(swapChain))
{
    createModels();
    createScenePipelines();
    connectPipelines();

    TurnTableCameraParams cameraParams{};
    cameraParams.target          = glm::vec3(0.f);
    cameraParams.initialRadius   = glm::length(glm::vec3(0.f, 20.f, 20.f));
    cameraParams.initialAzimuth  = 0.f;
    cameraParams.initialElevation = -glm::quarter_pi<float>(); // 45° below horizon
    _camera = std::make_unique<TurnTableCamera>(cameraParams);
}


ClothScene::~ClothScene()
{
    vkDeviceWaitIdle(_ctx->device);
    _renderPipeline.reset();
}


void ClothScene::advance()
{
    auto  now        = std::chrono::high_resolution_clock::now();
    float elapsedSec = std::chrono::duration<float>(now - _lastFrameTime).count();
    _lastFrameTime   = now;

    _sceneInfo.time += elapsedSec;
    _camera->advanceAnimation(elapsedSec);

    _clothModel->advance(elapsedSec);
}


void ClothScene::buildUI()
{
    ImGui::Begin("Cloth");
    ImGui::Text(ICON_FA_TACHOMETER_ALT " %.1f FPS  (%.2f ms)",
        ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    ImGui::Separator();
    ImGui::Text(ICON_FA_FILM  " Scene");
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
    ImGui::Text(ICON_FA_ATOM " Mass Spring");
    ImGui::Indent(16.0f);
    auto& sim = _clothModel->getSimulation();
    ImGui::SliderFloat("Stiffness",       &sim.stiffness,     0.f, 2000.f);
    ImGui::SliderFloat("Spring Damping",  &sim.springDamping, 0.f, 20.f);
    ImGui::SliderFloat("Velocity Damping",&sim.damping,       0.f, 1.f);
    ImGui::SliderInt  ("Substeps",        &sim.substeps,      1,   20);
    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text(ICON_FA_PROJECT_DIAGRAM " Structure");
    ImGui::Indent(16.0f);
    ImGui::Checkbox("Shear Springs", &sim.shearEnabled);
    ImGui::Checkbox("Bend Springs",  &sim.bendEnabled);
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


void ClothScene::createModels()
{
    _clothModel = std::make_shared<ClothModel>(_ctx, 21, 33, 0.5f); // 32/20 = 1.6 aspect ratio
    _clothModel->setTexture(AssetPath::getInstance()->get("textures/flag_iran.png"));
    _sceneModels.push_back(_clothModel);
}


void ClothScene::createScenePipelines()
{
    PipelineParams params;
    params.name                      = "ClothPipeline";
    params.vertexBindingDescription  = Vertex::getBindingDescription();
    params.vertexAttributeDescriptions = Vertex::getAttributeDescriptions();
    params.descriptorSetLayouts      = {
        _sceneDescriptorSets[0]->getDescriptorSetLayout(),
        _clothModel->getDescriptorSet()->getDescriptorSetLayout()
    };
    params.pushConstantRanges        = { {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)} };
    params.renderPass                = _mainRenderPass->getRenderPass();
    params.cullMode                  = VK_CULL_MODE_NONE; // cloth is visible from both sides
    params.msaaSamples               = _msaaSamples;

    _renderPipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/cloth/cloth_vert.spv"),
        AssetPath::getInstance()->get("spv/cloth/cloth_frag.spv"),
        params);

    params.name        = "ClothWireframePipeline";
    params.polygonMode = VK_POLYGON_MODE_LINE;
    _wireframePipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/cloth/cloth_vert.spv"),
        AssetPath::getInstance()->get("spv/cloth/cloth_frag.spv"),
        params);
}


void ClothScene::connectPipelines()
{
    _clothModel->setPipeline(_renderPipeline);
}
