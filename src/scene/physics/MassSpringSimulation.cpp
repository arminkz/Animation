#include "MassSpringSimulation.h"
#include "vulkan/VulkanHelper.h"

MassSpringSimulation::MassSpringSimulation(std::shared_ptr<VulkanContext> ctx)
    : _ctx(std::move(ctx))
{
}

void MassSpringSimulation::init(const std::vector<MSParticle>&        particles,
                                const std::vector<MSParticleMetadata>& metadata,
                                const std::vector<MSSpring>&           directedSprings,
                                const std::vector<uint32_t>&           indices,
                                const std::string&                     shaderSpvPath)
{
    _numParticles = static_cast<uint32_t>(particles.size());
    _indexCount   = static_cast<uint32_t>(indices.size());

    // --- Build per-vertex triangle adjacency list from index buffer ---
    uint32_t numTris = _indexCount / 3;
    std::vector<MSTriangle> triangles(numTris);
    for (uint32_t t = 0; t < numTris; ++t)
        triangles[t] = { indices[t*3], indices[t*3+1], indices[t*3+2] };

    std::vector<MSVertexTriMetadata> vertexTriMeta(_numParticles);
    std::vector<MSTriangle>          vertexTris;
    vertexTris.reserve(numTris * 3); // each triangle appears 3 times

    for (uint32_t v = 0; v < _numParticles; ++v)
    {
        vertexTriMeta[v].triStartIndex = static_cast<uint32_t>(vertexTris.size());
        for (const auto& tri : triangles)
            if (tri.a == v || tri.b == v || tri.c == v)
                vertexTris.push_back(tri);
        vertexTriMeta[v].triCount = static_cast<uint32_t>(vertexTris.size()) - vertexTriMeta[v].triStartIndex;
    }

    // --- Upload all buffers to device-local memory ---
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

    uploadDeviceLocal(indices.data(),         sizeof(uint32_t)           * indices.size(),         VK_BUFFER_USAGE_INDEX_BUFFER_BIT, _indexBuffer);
    uploadDeviceLocal(metadata.data(),        sizeof(MSParticleMetadata)  * metadata.size(),        0, _particleMetadataBuffer);
    uploadDeviceLocal(directedSprings.data(), sizeof(MSSpring)            * directedSprings.size(), 0, _springsBuffer);
    uploadDeviceLocal(vertexTriMeta.data(),   sizeof(MSVertexTriMetadata) * vertexTriMeta.size(),   0, _vertexTriMetaBuffer);
    uploadDeviceLocal(vertexTris.data(),      sizeof(MSTriangle)          * vertexTris.size(),      0, _trianglesBuffer);

    _paramsBuffer = std::make_unique<Buffer>(_ctx, sizeof(MSSimParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Collider buffer — host-visible, fixed capacity, zeroed initially
    _colliderBuffer = std::make_unique<Buffer>(_ctx, sizeof(MSCollider) * MAX_COLLIDERS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::vector<MSCollider> empty(MAX_COLLIDERS);
    _colliderBuffer->copyData(empty.data(), sizeof(MSCollider) * MAX_COLLIDERS);

    VkDescriptorBufferInfo paramsInfo      = _paramsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo metadataInfo    = _particleMetadataBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo springsInfo     = _springsBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo vtMetaInfo      = _vertexTriMetaBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo trianglesInfo   = _trianglesBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo collidersInfo   = _colliderBuffer->getDescriptorInfo();

    for (int i = 0; i < 2; ++i)
    {
        VkDescriptorBufferInfo inInfo  = _particleBuffers[i]->getDescriptorInfo();
        VkDescriptorBufferInfo outInfo = _particleBuffers[1 - i]->getDescriptorInfo();

        _descriptorSets[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, paramsInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, inInfo),
            Descriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, metadataInfo),
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, springsInfo),
            Descriptor(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, vtMetaInfo),
            Descriptor(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, trianglesInfo),
            Descriptor(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, collidersInfo),
            Descriptor(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, outInfo),
        });
    }

    _computePipeline = std::make_unique<ComputePipeline>(
        _ctx,
        shaderSpvPath,
        std::vector<VkDescriptorSetLayout>{ _descriptorSets[0]->getDescriptorSetLayout() }
    );
}


void MassSpringSimulation::dispatch(VkCommandBuffer cmd, float dt, float time)
{
    // Update UBO
    MSSimParams params{};
    params.numParticles    = static_cast<int32_t>(_numParticles);
    params.dt              = dt;
    params.time            = time;
    params.stiffness       = stiffness;
    params.velocityDamping = damping;
    params.springDamping   = springDamping;
    params.windDir         = glm::length(windDirection) > 1e-6f
                               ? glm::normalize(windDirection)
                               : glm::vec3(0.f, 0.f, 1.f);
    params.windEnabled     = windEnabled    ? 1.f : 0.f;
    params.windTurbulence  = windTurbulence;
    params.windStrength    = windStrength;
    params.windDragCoeff   = windDragCoeff;
    params.gravityEnabled  = gravityEnabled ? 1 : 0;
    params.numColliders    = _numColliders;

    _paramsBuffer->copyData(&params, sizeof(MSSimParams));

    _computePipeline->bind(cmd);

    VkDescriptorSet ds = _descriptorSets[_currentIn]->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        _computePipeline->getPipelineLayout(), 0, 1, &ds, 0, nullptr);

    uint32_t groups = (_numParticles + 63) / 64; // local size is 64
    vkCmdDispatch(cmd, groups, 1, 1);

    _currentIn ^= 1;
}


void MassSpringSimulation::setColliders(const std::vector<MSCollider>& colliders)
{
    _numColliders = static_cast<int32_t>(std::min((int)colliders.size(), MAX_COLLIDERS));
    if (_numColliders > 0)
        _colliderBuffer->copyData(colliders.data(), sizeof(MSCollider) * _numColliders);
}
