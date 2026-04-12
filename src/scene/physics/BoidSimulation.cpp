#include "BoidSimulation.h"
#include "vulkan/VulkanHelper.h"
#include "core/AssetPath.h"

BoidSimulation::BoidSimulation(std::shared_ptr<VulkanContext> ctx,
                               uint32_t nBoids,
                               const std::vector<BoidState>& initialState,
                               float wrapRange)
    : _ctx(std::move(ctx))
    , _nBoids(nBoids)
    , _wrapRange(wrapRange)
{
    buildParamsBuffer();
    buildBoidStateBuffers(initialState);
    buildTransformsBuffer();
    buildGridBuffers();

    buildDescriptorSets();
    buildPipelines();
}


void BoidSimulation::buildParamsBuffer() {
    // Params UBO (host-visible, updated every frame)
    _paramsBuffer = std::make_unique<Buffer>(
        _ctx, sizeof(BoidSimParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}


void BoidSimulation::buildBoidStateBuffers(const std::vector<BoidState>& initialState) {
    // Boid state ping-pong SSBO buffers (device-visible)
    VkDeviceSize stateSize = sizeof(BoidState) * _nBoids;

    auto uploadDeviceLocal = [&](const void* data, VkDeviceSize size,
                                  VkBufferUsageFlags extra,
                                  std::unique_ptr<Buffer>& out) {
        Buffer staging(_ctx, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.copyData(data, size);
        out = std::make_unique<Buffer>(_ctx, size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), out->getBuffer(), size);
    };

    uploadDeviceLocal(initialState.data(), stateSize, 0, _boidStateBuffers[0]);
    uploadDeviceLocal(initialState.data(), stateSize, 0, _boidStateBuffers[1]);
}


void BoidSimulation::buildTransformsBuffer() {
    // Transform SSBO buffer (mat4 per boid, written by compute, read by vertex shader)
    // No staging needed because the values are written by compute stage
    VkDeviceSize transformSize = sizeof(glm::mat4) * _nBoids;
    _transformsBuffer = std::make_unique<Buffer>(
        _ctx, transformSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

}


void BoidSimulation::buildGridBuffers() {
    // Spatial Grid SSBOs
    // Grid size should be bigger that all radii so that we are guaranteed 
    // to get all neighbour boids by checking distance 1 cells.
    float cellSize = std::max({ rs, ra, rc });
    
    _gridDim = static_cast<uint32_t>(std::ceil(2.f * _wrapRange / cellSize));
    _totalCells = _gridDim * _gridDim * _gridDim;

    VkDeviceSize countsSize = sizeof(uint32_t) * _totalCells;
    VkDeviceSize boidsSize  = sizeof(uint32_t) * _totalCells * MAX_PER_CELL;

    _gridCountsBuffer = std::make_unique<Buffer>(
        _ctx, countsSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    _gridBoidsBuffer = std::make_unique<Buffer>(
        _ctx, boidsSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}


void BoidSimulation::buildDescriptorSets()
{
    VkDescriptorBufferInfo paramsInfo = _paramsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo gridCountsInfo = _gridCountsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo gridBoidsInfo = _gridBoidsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo transformInfo = _transformsBuffer->getDescriptorInfo();

    // Clear Shader (boid_grid_clear) DS: params + gridCounts
    _clearDS = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
        Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
        Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, gridCountsInfo),
    });

    // Build Shader (boid_grid_build) DS: one per ping-pong (reads current in-state)
    for (int i = 0; i < 2; ++i) {
        VkDescriptorBufferInfo boidStateInfo = _boidStateBuffers[i]->getDescriptorInfo();
        _buildDS[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, boidStateInfo),
            Descriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, gridCountsInfo),
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, gridBoidsInfo),
        });
    }

    // Simulate Shader (boid_simulate) DS: ping-pong (reads in-state, writes out-state + transforms)
    for (int i = 0; i < 2; ++i) {
        VkDescriptorBufferInfo inBoidsInfo = _boidStateBuffers[i]->getDescriptorInfo();
        VkDescriptorBufferInfo outBoidsInfo = _boidStateBuffers[1 - i]->getDescriptorInfo();
        _simulateDS[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, inBoidsInfo),
            Descriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, gridCountsInfo),
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, gridBoidsInfo),
            Descriptor(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, outBoidsInfo),
            Descriptor(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, transformInfo),
        });
    }
}


void BoidSimulation::buildPipelines()
{
    auto* ap = AssetPath::getInstance();

    _clearPipeline = std::make_unique<ComputePipeline>(
        _ctx,
        ap->get("spv/compute/boid_grid_clear_comp.spv"),
        std::vector<VkDescriptorSetLayout>{ _clearDS->getDescriptorSetLayout() });

    _buildPipeline = std::make_unique<ComputePipeline>(
        _ctx,
        ap->get("spv/compute/boid_grid_build_comp.spv"),
        std::vector<VkDescriptorSetLayout>{ _buildDS[0]->getDescriptorSetLayout() });

    _simulatePipeline = std::make_unique<ComputePipeline>(
        _ctx,
        ap->get("spv/compute/boid_simulate_comp.spv"),
        std::vector<VkDescriptorSetLayout>{ _simulateDS[0]->getDescriptorSetLayout() });
}


void BoidSimulation::dispatch(VkCommandBuffer cmd, float dt)
{
    float cellSize = std::max({ rs, ra, rc });

    // Rebuild grid if cell size changed enough to alter gridDim
    uint32_t newGridDim = static_cast<uint32_t>(std::ceil(2.f * _wrapRange / cellSize));
    if (newGridDim != _gridDim) {
        vkQueueWaitIdle(_ctx->graphicsQueue); // flush before reallocating
        buildGridBuffers();
        buildDescriptorSets();
        buildPipelines();
    }

    // Update UBO
    BoidSimParams p{};
    p.nBoids = _nBoids;
    p.dt = dt;
    p.perceptionAngle = perceptionAngle;
    p.rs = rs;
    p.ra = ra;
    p.rc = rc;
    p.ws = ws;
    p.wa = wa;
    p.wc = wc;
    p.minSpeed = minSpeed;
    p.maxSpeed = maxSpeed;
    p.wrapRange = _wrapRange;
    p.cellSize = cellSize;
    p.gridDim = _gridDim;
    p.totalCells = _totalCells;
    _paramsBuffer->copyData(&p, sizeof(BoidSimParams));

    // Pass 1: clear grid counts
    _clearPipeline->bind(cmd);
    VkDescriptorSet clearDS = _clearDS->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _clearPipeline->getPipelineLayout(), 0, 1, &clearDS, 0, nullptr);
    vkCmdDispatch(cmd, (_totalCells + 63) / 64, 1, 1);
    VulkanHelper::barrierComputeToCompute(cmd);

    // Pass 2: build grid (insert each boid into its cell)
    _buildPipeline->bind(cmd);
    VkDescriptorSet buildDS = _buildDS[_currentIn]->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _buildPipeline->getPipelineLayout(), 0, 1, &buildDS, 0, nullptr);
    vkCmdDispatch(cmd, (_nBoids + 63) / 64, 1, 1);
    VulkanHelper::barrierComputeToCompute(cmd);

    // Pass 3: simulate (flocking + write transforms)
    _simulatePipeline->bind(cmd);
    VkDescriptorSet simulateDS = _simulateDS[_currentIn]->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _simulatePipeline->getPipelineLayout(), 0, 1, &simulateDS, 0, nullptr);
    vkCmdDispatch(cmd, (_nBoids + 63) / 64, 1, 1);

    _currentIn ^= 1; // ping the pong!
}


void BoidSimulation::resetBoids(const std::vector<BoidState>& state)
{
    VkDeviceSize stateSize = sizeof(BoidState) * _nBoids;

    auto reupload = [&](std::unique_ptr<Buffer>& buf) {
        Buffer staging(_ctx, stateSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.copyData(state.data(), stateSize);
        VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), buf->getBuffer(), stateSize);
    };

    vkQueueWaitIdle(_ctx->graphicsQueue);
    reupload(_boidStateBuffers[0]);
    reupload(_boidStateBuffers[1]);
    _currentIn = 0;
}
