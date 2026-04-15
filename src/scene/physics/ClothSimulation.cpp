#include "ClothSimulation.h"
#include "core/AssetPath.h"
#include "vulkan/VulkanHelper.h"

namespace {
struct PreRenderPC { uint32_t numVertices; };
}

ClothSimulation::ClothSimulation(std::shared_ptr<VulkanContext> ctx,
                                 int rows, int cols, float spacing,
                                 ClothPinMode pinMode,
                                 glm::mat4 initialTransform)
    : MassSpringSimulation(std::move(ctx))
{
    auto idx = [&](int r, int c) { return r * cols + c; };

    // Build particle grid
    std::vector<MSParticle> particles(rows * cols);
    std::vector<ClothVertex> renderVerts(rows * cols);
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            int i = idx(r, c);
            glm::vec3 pos(
                c * spacing - (cols - 1) * spacing * 0.5f,
                r * spacing - (rows - 1) * spacing * 0.5f,
                0.f
            );
            particles[i].position = pos;
            particles[i].prevPosition = pos;
            particles[i].normal = glm::vec3(0.f, 0.f, 1.f);
            bool pinned = (pinMode == ClothPinMode::LeftColumn && c == 0) ||
                          (pinMode == ClothPinMode::TopEdge && r == rows - 1);
            particles[i].invMass = pinned ? 0.f : 1.f;

            // UV lives in the render vertex, not the physics particle
            renderVerts[i].uv = glm::vec2((float)c / (cols - 1), 1.0f - (float)r / (rows - 1));
        }
    }

    // Build springs
    struct Spring { int32_t a, b; float restLength, ratio; };
    std::vector<Spring> springs;
    auto addSpring = [&](int a, int b, float ratio)
    {
        float rest = glm::length(particles[a].position - particles[b].position);
        springs.push_back({a, b, rest, ratio});
    };

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            if (c + 1 < cols) addSpring(idx(r, c), idx(r,   c+1), 1.0f);                 // Structural
            if (r + 1 < rows) addSpring(idx(r, c), idx(r+1, c  ), 1.0f);
            if (r + 1 < rows && c + 1 < cols) addSpring(idx(r, c), idx(r+1, c+1), 0.6f); // Shear
            if (r + 1 < rows && c - 1 >= 0  ) addSpring(idx(r, c), idx(r+1, c-1), 0.6f);
            if (c + 2 < cols) addSpring(idx(r, c), idx(r,   c+2), 0.4f);                 // Bend
            if (r + 2 < rows) addSpring(idx(r, c), idx(r+2, c  ), 0.4f);
        }
    }

    // Build triangle buffer (used for physics normal + index buffer)
    std::vector<uint32_t> triangleIndices;
    for (int r = 0; r < rows - 1; ++r)
    {
        for (int c = 0; c < cols - 1; ++c)
        {
            uint32_t tl = idx(r,   c), tr = idx(r,   c+1);
            uint32_t bl = idx(r+1, c), br = idx(r+1, c+1);
            triangleIndices.insert(triangleIndices.end(), {tl, bl, tr});
            triangleIndices.insert(triangleIndices.end(), {tr, bl, br});
        }
    }

    // Build directed spring adjacency list
    uint32_t numParticles = static_cast<uint32_t>(particles.size());
    std::vector<MSParticleSprRange> sprRanges(numParticles);
    std::vector<MSSpring> directedSprings;
    directedSprings.reserve(springs.size() * 2);

    for (uint32_t p = 0; p < numParticles; ++p)
    {
        sprRanges[p].springStartIndex = static_cast<uint32_t>(directedSprings.size());
        for (const auto& s : springs)
        {
            if (s.a == (int32_t)p)
                directedSprings.push_back({ s.b, s.restLength, s.ratio });
            else if (s.b == (int32_t)p)
                directedSprings.push_back({ s.a, s.restLength, s.ratio });
        }
        sprRanges[p].springCount = static_cast<uint32_t>(directedSprings.size()) - sprRanges[p].springStartIndex;
    }

    // Apply initial transform to particle positions and normals
    glm::mat3 normalMatrix = glm::mat3(initialTransform);
    for (auto& p : particles) {
        p.position = glm::vec3(initialTransform * glm::vec4(p.position, 1.f));
        p.prevPosition = p.position;
        p.normal = glm::normalize(normalMatrix * p.normal);
    }

    // Call init on parent after creating the topology.
    init(particles, sprRanges, directedSprings, triangleIndices);

    // Upload index buffer
    _indexCount = static_cast<uint32_t>(triangleIndices.size());
    VkDeviceSize idxSize = sizeof(uint32_t) * triangleIndices.size();
    {
        Buffer staging(_ctx, idxSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.copyData(triangleIndices.data(), idxSize);
        _indexBuffer = std::make_unique<Buffer>(_ctx, idxSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), _indexBuffer->getBuffer(), idxSize);
    }

    // Upload vertex buffer (UVs are static; position/normal zeroed, filled by compute each frame)
    _numVertices = numParticles;
    VkDeviceSize rvSize = sizeof(ClothVertex) * _numVertices;
    {
        Buffer staging(_ctx, rvSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.copyData(renderVerts.data(), rvSize);
        _vertexBuffer = std::make_unique<Buffer>(_ctx, rvSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), _vertexBuffer->getBuffer(), rvSize);
    }

    // Descriptor sets (one per physics ping-pong state)
    VkDescriptorBufferInfo rvInfo = _vertexBuffer->getDescriptorInfo();
    for (int i = 0; i < 2; ++i)
    {
        VkDescriptorBufferInfo physInfo{ getParticleBuffer(i), 0, VK_WHOLE_SIZE };
        _preRenderDescriptorSets[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, physInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, rvInfo),
        });
    }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(PreRenderPC);

    _preRenderPipeline = std::make_unique<ComputePipeline>(
        _ctx,
        AssetPath::getInstance()->get("spv/cloth/cloth_pre_render_comp.spv"),
        std::vector<VkDescriptorSetLayout>{ _preRenderDescriptorSets[0]->getDescriptorSetLayout() },
        std::vector<VkPushConstantRange>{ pcRange }
    );
}


void ClothSimulation::dispatchPreRender(VkCommandBuffer cmd)
{
    // Bind pipeline
    _preRenderPipeline->bind(cmd);

    // Bind descriptor set
    VkDescriptorSet ds = _preRenderDescriptorSets[_currentIn]->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        _preRenderPipeline->getPipelineLayout(), 0, 1, &ds, 0, nullptr);

    // Push constants
    PreRenderPC pc{ _numVertices };
    vkCmdPushConstants(cmd, _preRenderPipeline->getPipelineLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PreRenderPC), &pc);

    // Dispatch
    uint32_t groups = (_numVertices + 63) / 64;
    vkCmdDispatch(cmd, groups, 1, 1);
}
