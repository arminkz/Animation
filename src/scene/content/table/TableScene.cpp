#include "TableScene.h"
#include "scene/camera/TurnTableCamera.h"
#include "core/AssetPath.h"
#include "gui/FontAwesome.h"
#include "vulkan/VulkanHelper.h"
#include "scene/physics/MassSpringSimulation.h"
#include "geometry/Vertex.h"

TableScene::TableScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : SinglePassRenderer(std::move(ctx), std::move(swapChain))
{
    createModels();
    createScenePipelines();

    _clothModel->setPipeline(_clothRenderPipeline);
    _sphereModel->setPipeline(_spherePipeline);

    // Tune simulation for free-falling cloth
    auto& sim = *_clothModel->getSimulation();
    sim.windEnabled = false;
    sim.damping = 0.98f; 
    sim.springDamping = 2.0f;
    sim.stiffness = 800.f;
    sim.substeps = 12;

    // Setup camera
    TurnTableCameraParams cam{};
    cam.target = glm::vec3(0.f);
    cam.initialRadius = 60.f;
    cam.initialAzimuth = 0.f;
    cam.initialElevation = glm::radians(-60.f);
    _camera = std::make_unique<TurnTableCamera>(cam);
}

TableScene::~TableScene()
{
    vkDeviceWaitIdle(_ctx->device);
    _clothRenderPipeline.reset();
    _clothWireframePipeline.reset();
    _spherePipeline.reset();
}

void TableScene::createModels()
{
    // Sphere
    _sphereModel = std::make_shared<SphereModel>(_ctx, _sphereCenter, _sphereRadius);
    _sceneModels.push_back(_sphereModel);

    // Horizontal cloth above the sphere, no pinning, rotated to lie flat in XZ plane
    glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(_dropOffsetX, _sphereRadius + 6.f, 0.f))
                        * glm::rotate(glm::mat4(1.f), glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f));

    _clothModel = std::make_shared<ClothModel>(_ctx, 50, 50, 0.3f, ClothPinMode::None, transform);
    _clothModel->setTexture(AssetPath::getInstance()->get("textures/fabric.png"));
    _sceneModels.push_back(_clothModel);
}

void TableScene::createScenePipelines()
{
    // Cloth pipeline
    PipelineParams clothParams;
    clothParams.name                       = "TableClothPipeline";
    clothParams.vertexBindingDescription   = MSParticle::getBindingDescription();
    clothParams.vertexAttributeDescriptions = MSParticle::getAttributeDescriptions();
    clothParams.descriptorSetLayouts       = {
        _sceneDescriptorSets[0]->getDescriptorSetLayout(),
        _clothModel->getDescriptorSet()->getDescriptorSetLayout()
    };
    clothParams.pushConstantRanges  = { {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)} };
    clothParams.renderPass          = _mainRenderPass->getRenderPass();
    clothParams.cullMode            = VK_CULL_MODE_NONE;
    clothParams.msaaSamples         = _msaaSamples;

    _clothRenderPipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/cloth/cloth_vert.spv"),
        AssetPath::getInstance()->get("spv/cloth/cloth_frag.spv"),
        clothParams);

    clothParams.name        = "TableClothWireframePipeline";
    clothParams.polygonMode = VK_POLYGON_MODE_LINE;
    _clothWireframePipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/cloth/cloth_vert.spv"),
        AssetPath::getInstance()->get("spv/cloth/cloth_frag.spv"),
        clothParams);

    // Sphere pipeline
    PipelineParams sphereParams;
    sphereParams.name                        = "SpherePipeline";
    sphereParams.vertexBindingDescription    = Vertex::getBindingDescription();
    sphereParams.vertexAttributeDescriptions = Vertex::getAttributeDescriptions();
    sphereParams.descriptorSetLayouts        = { _sceneDescriptorSets[0]->getDescriptorSetLayout() };
    sphereParams.pushConstantRanges          = { {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)} };
    sphereParams.renderPass                  = _mainRenderPass->getRenderPass();
    sphereParams.cullMode                    = VK_CULL_MODE_BACK_BIT;
    sphereParams.msaaSamples                 = _msaaSamples;

    _spherePipeline = std::make_unique<Pipeline>(_ctx,
        AssetPath::getInstance()->get("spv/table/sphere_vert.spv"),
        AssetPath::getInstance()->get("spv/table/sphere_frag.spv"),
        sphereParams);
}

void TableScene::restartCloth()
{
    vkDeviceWaitIdle(_ctx->device);

    // Remove old cloth from scene models
    _sceneModels.erase(std::remove(_sceneModels.begin(), _sceneModels.end(), _clothModel), _sceneModels.end());

    // Snapshot current sim parameters before destroying the old cloth
    auto& oldSim        = *_clothModel->getSimulation();
    auto  savedStiffness     = oldSim.stiffness;
    auto  savedDamping       = oldSim.damping;
    auto  savedSpringDamping = oldSim.springDamping;
    auto  savedSubsteps      = oldSim.substeps;
    auto  savedWindEnabled   = oldSim.windEnabled;
    auto  savedWindStrength  = oldSim.windStrength;
    auto  savedWindTurbulence= oldSim.windTurbulence;
    auto  savedWindDragCoeff = oldSim.windDragCoeff;
    auto  savedWindDirection = oldSim.windDirection;
    auto  savedGravityEnabled= oldSim.gravityEnabled;

    // Recreate cloth and re-attach pipeline + texture
    glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(_dropOffsetX, _sphereRadius + 6.f, 0.f))
                        * glm::rotate(glm::mat4(1.f), glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f));
    _clothModel = std::make_shared<ClothModel>(_ctx, 25, 25, 0.6f, ClothPinMode::None, transform);
    _clothModel->setTexture(AssetPath::getInstance()->get("textures/fabric.png"));
    _clothModel->setPipeline(_wireframe ? _clothWireframePipeline : _clothRenderPipeline);

    // Restore saved parameters
    auto& sim           = *_clothModel->getSimulation();
    sim.stiffness       = savedStiffness;
    sim.damping         = savedDamping;
    sim.springDamping   = savedSpringDamping;
    sim.substeps        = savedSubsteps;
    sim.windEnabled     = savedWindEnabled;
    sim.windStrength    = savedWindStrength;
    sim.windTurbulence  = savedWindTurbulence;
    sim.windDragCoeff   = savedWindDragCoeff;
    sim.windDirection   = savedWindDirection;
    sim.gravityEnabled  = savedGravityEnabled;

    _sceneModels.push_back(_clothModel);
}

void TableScene::advance()
{
    auto now = std::chrono::high_resolution_clock::now();
    _dt = std::min(std::chrono::duration<float>(now - _lastFrameTime).count(), 1.f / 30.f);
    _lastFrameTime = now;

    if (_paused) _dt = 0.f;

    _sceneInfo.time += _dt;
    _camera->advanceAnimation(_dt);
}

void TableScene::dispatchCompute(VkCommandBuffer cmd)
{
    if (_paused) return;

    ClothSimulation* sim = _clothModel->getSimulation();

    MSCollider sphere{};
    sphere.type      = static_cast<int32_t>(MSColliderType::Sphere);
    sphere.position  = _sphereCenter;
    sphere.radius    = _sphereRadius + _colliderOffset;
    sphere.stiffness = _colliderStiffness;
    sphere.friction  = _colliderFriction;
    sim->setColliders({ sphere });

    float subDt = _dt * _timeScale / static_cast<float>(sim->substeps);
    for (int s = 0; s < sim->substeps; ++s)
    {
        sim->dispatch(cmd, subDt, _sceneInfo.time);
        if (s < sim->substeps - 1)
            VulkanHelper::barrierComputeToCompute(cmd);
    }
    VulkanHelper::barrierComputeToVertex(cmd, sim->getOutParticleBuffer());
}

void TableScene::buildUI()
{
    ImGui::Begin("Cloth");
    ImGui::Text(ICON_FA_GAUGE " %.1f FPS  (%.2f ms)",
        ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    auto& sim = *_clothModel->getSimulation();

    ImGui::Separator();
    ImGui::Text(ICON_FA_FLASK " Simulation");
    ImGui::Indent(16.0f);
    if (ImGui::Button(_paused ? ICON_FA_PLAY " Resume" : ICON_FA_PAUSE " Pause"))
        _paused = !_paused;
    ImGui::SameLine();
    if (ImGui::Button("Restart")) restartCloth();
    ImGui::SliderFloat("Time Scale", &_timeScale, 0.1f, 5.f);
    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text(ICON_FA_SHIRT " Cloth");
    ImGui::Indent(16.0f);
    ImGui::SliderFloat("Initial X Offset", &_dropOffsetX, -5.f, 5.f);
    if (ImGui::Checkbox("Wireframe", &_wireframe))
        _clothModel->setPipeline(_wireframe ? _clothWireframePipeline : _clothRenderPipeline);
    ImGui::Unindent(16.0f);
    
    ImGui::Separator();
    ImGui::Text(ICON_FA_WEIGHT_HANGING " Mass Spring");
    ImGui::Indent(16.0f);
    ImGui::SliderFloat("Stiffness",        &sim.stiffness,      0.f, 3000.f);
    ImGui::SliderFloat("Spring Damping",   &sim.springDamping,  0.f, 20.f);
    ImGui::SliderFloat("Velocity Damping", &sim.damping,        0.f, 1.f);
    ImGui::SliderInt  ("Substeps",         &sim.substeps,       1,   20);
    ImGui::Unindent(16.0f);

    ImGui::Separator();
    ImGui::Text(ICON_FA_CIRCLE "Sphere Collider");
    ImGui::Indent(16.0f);
    ImGui::DragFloat3("Center",     &_sphereCenter.x,    0.1f);
    ImGui::SliderFloat("Radius",         &_sphereRadius,       0.5f,  15.f);
    ImGui::SliderFloat("Collider Offset",&_colliderOffset,     0.0f,  1.f);
    ImGui::SliderFloat("Col. Stiffness", &_colliderStiffness,  100.f, 20000.f);
    ImGui::SliderFloat("Friction",       &_colliderFriction,   0.f,   2.f);
    ImGui::Unindent(16.0f);

    ImGui::End();
}
