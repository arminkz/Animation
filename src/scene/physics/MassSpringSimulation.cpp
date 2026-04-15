#include "MassSpringSimulation.h"
#include "vulkan/VulkanHelper.h"
#include "core/AssetPath.h"

MassSpringSimulation::MassSpringSimulation(std::shared_ptr<VulkanContext> ctx)
    : _ctx(std::move(ctx))
{
}

void MassSpringSimulation::init(const std::vector<MSParticle>& particles,
                                const std::vector<MSParticleSprRange>& sprRanges,
                                const std::vector<MSSpring>& directedSprings,
                                const std::vector<uint32_t>& triangleIndices)
{
    _numParticles = static_cast<uint32_t>(particles.size());

    // Build per-vertex triangle adjacency list (used for wind-drag normals)
    uint32_t numTris = static_cast<uint32_t>(triangleIndices.size()) / 3;
    std::vector<MSTriangle> triangles(numTris);
    for (uint32_t t = 0; t < numTris; ++t)
        triangles[t] = { triangleIndices[t*3], triangleIndices[t*3+1], triangleIndices[t*3+2] };

    std::vector<MSParticleTriRange> particleTriRanges(_numParticles);
    std::vector<MSTriangle> particleTris;
    particleTris.reserve(numTris * 3); // each triangle appears 3 times

    // Go over each particle and set the affiliated tris
    for (uint32_t v = 0; v < _numParticles; ++v) { 
        particleTriRanges[v].triStartIndex = static_cast<uint32_t>(particleTris.size());
        for (const auto& tri : triangles)
            if (tri.a == v || tri.b == v || tri.c == v)
                particleTris.push_back(tri);
        particleTriRanges[v].triCount = static_cast<uint32_t>(particleTris.size()) - particleTriRanges[v].triStartIndex;
    }

    // Upload buffers to device-local memory (Host -> Staging -> Device)
    auto uploadDeviceLocal = [&](const void* data, VkDeviceSize size, VkBufferUsageFlags extraUsage, std::unique_ptr<Buffer>& outBuf) {
        Buffer staging(_ctx, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.copyData(data, size);
        outBuf = std::make_unique<Buffer>(_ctx, size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extraUsage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), outBuf->getBuffer(), size);
    };

    VkDeviceSize particleSize = sizeof(MSParticle) * _numParticles;
    for (int i = 0; i < 2; ++i)
        uploadDeviceLocal(particles.data(), particleSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, _particleBuffers[i]);

    uploadDeviceLocal(sprRanges.data(), sizeof(MSParticleSprRange) * sprRanges.size(), 0, _sprRangesBuffer);
    uploadDeviceLocal(directedSprings.data(), sizeof(MSSpring) * directedSprings.size(), 0, _springsBuffer);
    uploadDeviceLocal(particleTriRanges.data(), sizeof(MSParticleTriRange) * particleTriRanges.size(), 0, _triRangesBuffer);
    uploadDeviceLocal(particleTris.data(), sizeof(MSTriangle) * particleTris.size(), 0, _trianglesBuffer);

    // Params buffer is host-visible
    _paramsBuffer = std::make_unique<Buffer>(_ctx, sizeof(MSSimParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Collider buffer is host-visible, fixed capacity, zeroed initially
    _colliderBuffer = std::make_unique<Buffer>(_ctx, sizeof(MSCollider) * MAX_COLLIDERS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::vector<MSCollider> empty(MAX_COLLIDERS);
    _colliderBuffer->copyData(empty.data(), sizeof(MSCollider) * MAX_COLLIDERS);

    // Descriptor Sets
    VkDescriptorBufferInfo paramsInfo = _paramsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo sprRangesInfo = _sprRangesBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo springsInfo = _springsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo triRangesInfo = _triRangesBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo trianglesInfo = _trianglesBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo collidersInfo = _colliderBuffer->getDescriptorInfo();

    for (int i = 0; i < 2; ++i)
    {
        VkDescriptorBufferInfo inInfo  = _particleBuffers[i]->getDescriptorInfo();
        VkDescriptorBufferInfo outInfo = _particleBuffers[1-i]->getDescriptorInfo();

        _descriptorSets[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, inInfo),
            Descriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, sprRangesInfo),
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, springsInfo),
            Descriptor(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, triRangesInfo),
            Descriptor(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, trianglesInfo),
            Descriptor(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, collidersInfo),
            Descriptor(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, outInfo),
        });
    }

    // Compute Pipeline
    _computePipeline = std::make_unique<ComputePipeline>(
        _ctx,
        AssetPath::getInstance()->get("spv/mass_spring/mass_spring_integrate_comp.spv"),
        std::vector<VkDescriptorSetLayout>{ _descriptorSets[0]->getDescriptorSetLayout() }
    );
}


void MassSpringSimulation::dispatch(VkCommandBuffer cmd, float dt, float time)
{
    // Update UBO
    MSSimParams params{};
    params.numParticles = static_cast<int32_t>(_numParticles);
    params.numColliders = static_cast<int32_t>(_numColliders);
    params.dt = dt;
    params.time = time;
    params.stiffness = stiffness;
    params.velocityDamping = velocityDamping;
    params.springDamping = springDamping;
    params.gravityEnabled = gravityEnabled ? 1 : 0;
    params.windEnabled = windEnabled ? 1 : 0;
    params.windTurbulence = windTurbulence;
    params.windStrength = windStrength;
    params.windDragCoeff = windDragCoeff;
    params.windDir = glm::length(windDirection) > 1e-6f
                        ? glm::normalize(windDirection)
                        : glm::vec3(0.f, 0.f, 1.f);

    _paramsBuffer->copyData(&params, sizeof(MSSimParams));


    // Bind Compute Pipeline
    _computePipeline->bind(cmd);

    // Bind DescriptorSet based on ping-pong state
    VkDescriptorSet ds = _descriptorSets[_currentIn]->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        _computePipeline->getPipelineLayout(), 0, 1, &ds, 0, nullptr);

    // Dispatch
    uint32_t groups = (_numParticles + 63) / 64; // local size is 64
    vkCmdDispatch(cmd, groups, 1, 1);

    // Ping -> Pong
    _currentIn ^= 1;
}


void MassSpringSimulation::setColliders(const std::vector<MSCollider>& colliders)
{
    _numColliders = static_cast<int32_t>(std::min((int)colliders.size(), MAX_COLLIDERS));
    if (_numColliders > 0)
        _colliderBuffer->copyData(colliders.data(), sizeof(MSCollider) * _numColliders);
}
