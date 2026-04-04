#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/ComputePipeline.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/resources/Buffer.h"

// GPU-side boid state — 32 bytes, scalar layout compatible
struct BoidState {
    glm::vec3 position;
    float     _p0 = 0.f;
    glm::vec3 velocity;
    float     _p1 = 0.f;
};

// Must match the UBO layout in all three boid compute shaders exactly
struct BoidSimParams {
    uint32_t nBoids;
    float    dt;
    float    rs;          // separation radius
    float    ra;          // alignment  radius
    float    rc;          // cohesion   radius
    float    ws;          // separation weight
    float    wa;          // alignment  weight
    float    wc;          // cohesion   weight
    float    minSpeed;
    float    maxSpeed;
    float    wrapRange;   // half-extent of spawn volume (SPAWN_RANGE)
    float    cellSize;    // = max(rs, ra, rc)
    uint32_t gridDim;     // cells per axis
    uint32_t totalCells;  // gridDim^3
};

class BoidSimulation
{
public:
    BoidSimulation(std::shared_ptr<VulkanContext> ctx,
                   uint32_t nBoids,
                   const std::vector<BoidState>& initialState,
                   float wrapRange);
    ~BoidSimulation() = default;

    // Record one simulation step into cmd.
    void dispatch(VkCommandBuffer cmd, float dt);

    // Update flocking parameters (applied on next dispatch).
    void setParams(float rs, float ra, float rc,
                   float ws, float wa, float wc,
                   float minSpeed, float maxSpeed);

    // Re-upload initial boid positions/velocities (e.g. on reset).
    void resetBoids(const std::vector<BoidState>& state);

    // The GPU buffer containing mat4 transforms for the current frame.
    // Bind as SSBO in vertex shader indexed by gl_InstanceIndex.
    VkBuffer getTransformBuffer() const { return _transformBuffer->getBuffer(); }

    uint32_t getBoidCount() const { return _nBoids; }

    // Public params — set before each dispatch
    float rs = 4.f, ra = 15.f, rc = 15.f;
    float ws = 1.f, wa = 0.4f, wc = 0.2f;
    float minSpeed = 8.f, maxSpeed = 18.f;

private:
    std::shared_ptr<VulkanContext> _ctx;
    uint32_t _nBoids;
    float    _wrapRange;
    int      _currentIn = 0;

    // --- GPU buffers ---
    std::unique_ptr<Buffer> _paramsBuffer;        // host-visible UBO
    std::unique_ptr<Buffer> _stateBuffers[2];     // ping-pong BoidState[]
    std::unique_ptr<Buffer> _transformBuffer;     // mat4[nBoids] → vertex shader
    std::unique_ptr<Buffer> _gridCountsBuffer;    // uint[totalCells]
    std::unique_ptr<Buffer> _gridBoidsBuffer;     // uint[totalCells * MAX_PER_CELL]

    // --- Pipelines ---
    std::unique_ptr<ComputePipeline> _clearPipeline;
    std::unique_ptr<ComputePipeline> _buildPipeline;
    std::unique_ptr<ComputePipeline> _simulatePipeline;

    // --- Descriptor sets ---
    std::unique_ptr<DescriptorSet> _clearDS;      // one (grid buffers)
    std::unique_ptr<DescriptorSet> _buildDS[2];   // ping-pong (reads current in-buffer)
    std::unique_ptr<DescriptorSet> _simulateDS[2];// ping-pong (reads in, writes out)

    // --- Grid state ---
    static constexpr uint32_t MAX_PER_CELL = 64;
    uint32_t _gridDim    = 0;
    uint32_t _totalCells = 0;

    void buildGrid(float cellSize);
    void buildDescriptorSets();
    void buildPipelines();
};
