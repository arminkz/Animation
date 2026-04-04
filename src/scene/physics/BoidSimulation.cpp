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
    // --- Params UBO (host-visible, updated every frame) ---
    _paramsBuffer = std::make_unique<Buffer>(
        _ctx, sizeof(BoidSimParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // --- Boid state ping-pong buffers ---
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

    uploadDeviceLocal(initialState.data(), stateSize, 0, _stateBuffers[0]);
    uploadDeviceLocal(initialState.data(), stateSize, 0, _stateBuffers[1]);

    // --- Transform buffer (mat4 per boid, written by compute, read by vertex shader) ---
    VkDeviceSize transformSize = sizeof(glm::mat4) * _nBoids;
    _transformBuffer = std::make_unique<Buffer>(
        _ctx, transformSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // --- Grid: allocate based on default radii ---
    float cellSize = std::max({ rs, ra, rc });
    buildGrid(cellSize);

    buildDescriptorSets();
    buildPipelines();
}


void BoidSimulation::buildGrid(float cellSize)
{
    _gridDim    = static_cast<uint32_t>(std::ceil(2.f * _wrapRange / cellSize));
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
    VkDescriptorBufferInfo paramsInfo  = _paramsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo countsInfo  = _gridCountsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo boidsGridInfo = _gridBoidsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo transformInfo = _transformBuffer->getDescriptorInfo();

    // --- Clear DS: params + gridCounts ---
    _clearDS = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
        Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
        Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, countsInfo),
    });

    // --- Build DS: one per ping-pong (reads current in-state) ---
    for (int i = 0; i < 2; ++i) {
        VkDescriptorBufferInfo stateInfo = _stateBuffers[i]->getDescriptorInfo();
        _buildDS[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, stateInfo),
            Descriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, countsInfo),
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, boidsGridInfo),
        });
    }

    // --- Simulate DS: ping-pong (reads in-state, writes out-state + transforms) ---
    for (int i = 0; i < 2; ++i) {
        VkDescriptorBufferInfo inInfo  = _stateBuffers[i]->getDescriptorInfo();
        VkDescriptorBufferInfo outInfo = _stateBuffers[1 - i]->getDescriptorInfo();
        _simulateDS[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, inInfo),
            Descriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, outInfo),
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, countsInfo),
            Descriptor(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  VK_SHADER_STAGE_COMPUTE_BIT, 1, boidsGridInfo),
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
        buildGrid(cellSize);
        buildDescriptorSets();
        buildPipelines();
    }

    // Update UBO
    BoidSimParams p{};
    p.nBoids     = _nBoids;
    p.dt         = dt;
    p.rs         = rs;
    p.ra         = ra;
    p.rc         = rc;
    p.ws         = ws;
    p.wa         = wa;
    p.wc         = wc;
    p.minSpeed   = minSpeed;
    p.maxSpeed   = maxSpeed;
    p.wrapRange  = _wrapRange;
    p.cellSize   = cellSize;
    p.gridDim    = _gridDim;
    p.totalCells = _totalCells;
    _paramsBuffer->copyData(&p, sizeof(BoidSimParams));

    auto bindDS = [&](ComputePipeline* pipeline, VkDescriptorSet ds) {
        pipeline->bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline->getPipelineLayout(), 0, 1, &ds, 0, nullptr);
    };

    // Pass 1: clear grid counts
    bindDS(_clearPipeline.get(), _clearDS->getDescriptorSet());
    vkCmdDispatch(cmd, (_totalCells + 63) / 64, 1, 1);
    VulkanHelper::barrierComputeToCompute(cmd);

    // Pass 2: build grid (insert each boid into its cell)
    bindDS(_buildPipeline.get(), _buildDS[_currentIn]->getDescriptorSet());
    vkCmdDispatch(cmd, (_nBoids + 63) / 64, 1, 1);
    VulkanHelper::barrierComputeToCompute(cmd);

    // Pass 3: simulate (flocking + write transforms)
    bindDS(_simulatePipeline.get(), _simulateDS[_currentIn]->getDescriptorSet());
    vkCmdDispatch(cmd, (_nBoids + 63) / 64, 1, 1);

    _currentIn ^= 1;
}


void BoidSimulation::setParams(float rs_, float ra_, float rc_,
                                float ws_, float wa_, float wc_,
                                float minSpeed_, float maxSpeed_)
{
    rs = rs_; ra = ra_; rc = rc_;
    ws = ws_; wa = wa_; wc = wc_;
    minSpeed = minSpeed_;
    maxSpeed = maxSpeed_;
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
    reupload(_stateBuffers[0]);
    reupload(_stateBuffers[1]);
    _currentIn = 0;
}
