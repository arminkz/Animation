#include "JellySimulation.h"
#include "core/AssetPath.h"
#include "vulkan/VulkanHelper.h"

namespace {

// Push constant for the render-normals compute shader.
struct RenderNormalsPC {
    uint32_t numRenderVerts;
};

} // namespace

JellySimulation::JellySimulation(std::shared_ptr<VulkanContext> ctx,
                                 int resX, int resY, int resZ,
                                 float spacing,
                                 glm::vec3 center)
    : MassSpringSimulation(std::move(ctx))
{
    auto idx = [&](int x, int y, int z) { return x + y * resX + z * resX * resY; };
    int numParticles = resX * resY * resZ;

    // Build particle grid
    std::vector<MSParticle> particles(numParticles);
    for (int z = 0; z < resZ; ++z)
    for (int y = 0; y < resY; ++y)
    for (int x = 0; x < resX; ++x)
    {
        int i = idx(x, y, z);
        glm::vec3 pos = center + glm::vec3(
            (x - (resX - 1) * 0.5f) * spacing,
            (y - (resY - 1) * 0.5f) * spacing,
            (z - (resZ - 1) * 0.5f) * spacing
        );
        particles[i].position = pos;
        particles[i].prevPosition = pos;
        particles[i].normal = glm::vec3(0.f, 1.f, 0.f);
        particles[i].invMass = 1.f;
    }

    // Build springs
    struct SpringDef { int dx, dy, dz; float ratio; };
    std::vector<SpringDef> springDefs = {
        // Structural (face neighbors)
        {1,0,0,1.f},{0,1,0,1.f},{0,0,1,1.f},
        // Shear (face diagonals)
        {1,1,0,.8f},{1,-1,0,.8f},{1,0,1,.8f},{1,0,-1,.8f},{0,1,1,.8f},{0,1,-1,.8f},
        // Body diagonals
        {1,1,1,.6f},{1,1,-1,.6f},{1,-1,1,.6f},{1,-1,-1,.6f},
        // Bend (distance 2)
        {2,0,0,.4f},{0,2,0,.4f},{0,0,2,.4f},
    };

    struct Spring { int32_t a, b; float restLength, ratio; };
    std::vector<Spring> cpuSprings;

    for (int z = 0; z < resZ; ++z)
    for (int y = 0; y < resY; ++y)
    for (int x = 0; x < resX; ++x)
    {
        int a = idx(x, y, z);
        for (auto& d : springDefs)
        {
            int nx = x + d.dx, ny = y + d.dy, nz = z + d.dz;
            if (nx < 0 || nx >= resX || ny < 0 || ny >= resY || nz < 0 || nz >= resZ) continue;
            int b = idx(nx, ny, nz);
            float rest = glm::length(particles[a].position - particles[b].position);
            cpuSprings.push_back({a, b, rest, d.ratio});
        }
    }

    // Build triangle buffer (used for physics normal only)
    std::vector<uint32_t> triangleIndices;
    auto addQuad = [&](int a, int b, int c, int d) {
        triangleIndices.insert(triangleIndices.end(), {(uint32_t)a, (uint32_t)b, (uint32_t)c});
        triangleIndices.insert(triangleIndices.end(), {(uint32_t)a, (uint32_t)c, (uint32_t)d});
    };

    for (int y = 0; y < resY - 1; ++y)
    for (int x = 0; x < resX - 1; ++x)
    {
        // z=0 face (normal -z): winding gives cross pointing -z
        addQuad(idx(x,y,0), idx(x,y+1,0), idx(x+1,y+1,0), idx(x+1,y,0));
        // z=resZ-1 face (normal +z)
        addQuad(idx(x,y,resZ-1), idx(x+1,y,resZ-1), idx(x+1,y+1,resZ-1), idx(x,y+1,resZ-1));
    }
    for (int z = 0; z < resZ - 1; ++z)
    for (int x = 0; x < resX - 1; ++x)
    {
        // y=0 face (normal -y)
        addQuad(idx(x,0,z), idx(x+1,0,z), idx(x+1,0,z+1), idx(x,0,z+1));
        // y=resY-1 face (normal +y)
        addQuad(idx(x,resY-1,z), idx(x,resY-1,z+1), idx(x+1,resY-1,z+1), idx(x+1,resY-1,z));
    }
    for (int z = 0; z < resZ - 1; ++z)
    for (int y = 0; y < resY - 1; ++y)
    {
        // x=0 face (normal -x)
        addQuad(idx(0,y,z), idx(0,y,z+1), idx(0,y+1,z+1), idx(0,y+1,z));
        // x=resX-1 face (normal +x)
        addQuad(idx(resX-1,y,z), idx(resX-1,y+1,z), idx(resX-1,y+1,z+1), idx(resX-1,y,z+1));
    }

    // Build directed spring adjacency list
    std::vector<MSParticleSprRange> sprRanges(numParticles);
    std::vector<MSSpring> directedSprings;
    directedSprings.reserve(cpuSprings.size() * 2);

    for (int p = 0; p < numParticles; ++p)
    {
        sprRanges[p].springStartIndex = static_cast<uint32_t>(directedSprings.size());
        for (auto& s : cpuSprings)
        {
            if (s.a == p) directedSprings.push_back({s.b, s.restLength, s.ratio});
            else if (s.b == p) directedSprings.push_back({s.a, s.restLength, s.ratio});
        }
        sprRanges[p].springCount = static_cast<uint32_t>(directedSprings.size()) - sprRanges[p].springStartIndex;
    }

    // call init on parent after topology is created.
    init(particles, sprRanges, directedSprings, triangleIndices);

    // ---- Per-face render mesh ----
    // Every surface vertex is duplicated per face so that edge/corner
    // particles get a face-local normal instead of an averaged one. Each
    // render vertex stores its source physics particle index so the compute
    // pass can pull the latest physics position from the ping-pong buffer.
    std::vector<JellyVertex> renderVertices;
    std::vector<uint32_t> renderIndices;
    std::vector<MSTriangle> renderTriangles;
    std::vector<std::vector<uint32_t>> adjacency; // per render-vertex → triangle triangleIndices

    // Build one cube face at a time.
    //  uMax/vMax : grid dimensions along the face
    //  getPhys   : (u,v) → physics particle index (shared buffer)
    //  flipWind  : if true the quad diagonal is reversed so cross product points outward
    auto buildFace = [&](int uMax, int vMax,
                         auto getPhys, bool flipWind)
    {
        uint32_t baseRv = static_cast<uint32_t>(renderVertices.size());

        for (int v = 0; v < vMax; ++v)
        for (int u = 0; u < uMax; ++u)
        {
            JellyVertex rv{};
            rv.position = glm::vec3(0.f);           // filled in each frame by compute
            rv.normal = glm::vec3(0.f, 1.f, 0.f);   // filled in each frame by compute
            rv.uv = glm::vec2((float)u / (uMax - 1), (float)v / (vMax - 1));
            rv.physicsIdx = static_cast<uint32_t>(getPhys(u, v));
            renderVertices.push_back(rv);
            adjacency.emplace_back();
        }

        auto rvIdx = [&](int u, int v) { return baseRv + v * uMax + u; };

        for (int v = 0; v < vMax - 1; ++v)
        for (int u = 0; u < uMax - 1; ++u)
        {
            uint32_t a = rvIdx(u,     v);
            uint32_t b = rvIdx(u + 1, v);
            uint32_t c = rvIdx(u + 1, v + 1);
            uint32_t d = rvIdx(u,     v + 1);

            // Two triangles with matching winding. The render-vertex index
            // buffer drives rasterization; the triangle list (using the
            // same render-vertex triangleIndices, but then replaced with physics
            // triangleIndices before upload) drives the compute-shader adjacency.
            uint32_t t0[3], t1[3];
            if (!flipWind) {
                t0[0] = a; t0[1] = b; t0[2] = c;
                t1[0] = a; t1[1] = c; t1[2] = d;
            } else {
                t0[0] = a; t0[1] = c; t0[2] = b;
                t1[0] = a; t1[1] = d; t1[2] = c;
            }

            auto emitTri = [&](uint32_t i0, uint32_t i1, uint32_t i2)
            {
                renderIndices.push_back(i0);
                renderIndices.push_back(i1);
                renderIndices.push_back(i2);

                // Store triangle with *physics* triangleIndices for the adjacency
                // buffer — the compute shader reads physics positions.
                MSTriangle tri{ renderVertices[i0].physicsIdx,
                                renderVertices[i1].physicsIdx,
                                renderVertices[i2].physicsIdx };
                uint32_t triGlobal = static_cast<uint32_t>(renderTriangles.size());
                renderTriangles.push_back(tri);

                // Record adjacency under each of the three *render-vertex*
                // corners so face-local normals stay per-face.
                adjacency[i0].push_back(triGlobal);
                adjacency[i1].push_back(triGlobal);
                adjacency[i2].push_back(triGlobal);
            };

            emitTri(t0[0], t0[1], t0[2]);
            emitTri(t1[0], t1[1], t1[2]);
        }
    };

    // z=0 face (normal -z). Winding was (a,b,c),(a,c,d) with
    //   a=(x,y,0), b=(x,y+1,0), c=(x+1,y+1,0), d=(x+1,y,0).
    // Map u→x, v→y but traverse so that (u,v)→(u+1,v)→(u+1,v+1)→(u,v+1)
    // matches the original quad (a,d,c,b) — i.e. need flipWind = true.
    buildFace(resX, resY,
              [&](int u, int v) { return idx(u, v, 0); },
              /*flipWind=*/true);

    // z=resZ-1 face (normal +z). Original: (x,y,resZ-1),(x+1,y,resZ-1),
    //   (x+1,y+1,resZ-1),(x,y+1,resZ-1) — standard CCW, flipWind=false.
    buildFace(resX, resY,
              [&](int u, int v) { return idx(u, v, resZ - 1); },
              /*flipWind=*/false);

    // y=0 face (normal -y). Original: (x,0,z),(x+1,0,z),(x+1,0,z+1),(x,0,z+1)
    // u→x, v→z, flipWind=false.
    buildFace(resX, resZ,
              [&](int u, int v) { return idx(u, 0, v); },
              /*flipWind=*/false);

    // y=resY-1 face (normal +y). Original: (x,resY-1,z),(x,resY-1,z+1),
    //   (x+1,resY-1,z+1),(x+1,resY-1,z).
    // Mapping u→x, v→z gives natural quad (a,b,c,d)=((u,v),(u+1,v),(u+1,v+1),(u,v+1))
    // which corresponds to (x,resY-1,z),(x+1,resY-1,z),(x+1,resY-1,z+1),(x,resY-1,z+1)
    // — that's the *reverse* of the original quad, so flipWind=true.
    buildFace(resX, resZ,
              [&](int u, int v) { return idx(u, resY - 1, v); },
              /*flipWind=*/true);

    // x=0 face (normal -x). Original: (0,y,z),(0,y,z+1),(0,y+1,z+1),(0,y+1,z).
    // u→y, v→z: quad is ((u,v),(u+1,v),(u+1,v+1),(u,v+1))
    // = (0,u,v),(0,u+1,v),(0,u+1,v+1),(0,u,v+1)
    // — reverse of the original, so flipWind=true.
    buildFace(resY, resZ,
              [&](int u, int v) { return idx(0, u, v); },
              /*flipWind=*/true);

    // x=resX-1 face (normal +x). Original: (resX-1,y,z),(resX-1,y+1,z),
    //   (resX-1,y+1,z+1),(resX-1,y,z+1).
    // u→y, v→z: quad is ((u,v),(u+1,v),(u+1,v+1),(u,v+1))
    // = (resX-1,u,v),(resX-1,u+1,v),(resX-1,u+1,v+1),(resX-1,u,v+1)
    // — matches original CCW, flipWind=false.
    buildFace(resY, resZ,
              [&](int u, int v) { return idx(resX - 1, u, v); },
              /*flipWind=*/false);

    // Flatten adjacency into (ranges, contiguous index list).
    std::vector<MSParticleTriRange> renderVertexTriRanges(renderVertices.size());
    std::vector<MSTriangle> renderAdjacencyTris;
    renderAdjacencyTris.reserve(renderTriangles.size() * 3);

    for (size_t v = 0; v < renderVertices.size(); ++v)
    {
        renderVertexTriRanges[v].triStartIndex = static_cast<uint32_t>(renderAdjacencyTris.size());
        for (uint32_t triGlobal : adjacency[v])
            renderAdjacencyTris.push_back(renderTriangles[triGlobal]);
        renderVertexTriRanges[v].triCount =
            static_cast<uint32_t>(renderAdjacencyTris.size()) - renderVertexTriRanges[v].triStartIndex;
    }

    initPreRenderBuffers(renderVertices, renderIndices, renderAdjacencyTris, renderVertexTriRanges);
}


void JellySimulation::initPreRenderBuffers(const std::vector<JellyVertex>& renderVertices,
                                           const std::vector<uint32_t>& renderIndices,
                                           const std::vector<MSTriangle>& renderTriangles,
                                           const std::vector<MSParticleTriRange>& renderVertexTriRanges)
{
    _numRenderVertices = static_cast<uint32_t>(renderVertices.size());
    _indexCount = static_cast<uint32_t>(renderIndices.size());

    // Device-local upload helper (mirrors MassSpringSimulation::init).
    auto uploadDeviceLocal = [&](const void* data, VkDeviceSize size,
                                 VkBufferUsageFlags extraUsage,
                                 std::unique_ptr<Buffer>& outBuf)
    {
        Buffer staging(_ctx, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.copyData(data, size);
        outBuf = std::make_unique<Buffer>(_ctx, size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extraUsage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), outBuf->getBuffer(), size);
    };

    uploadDeviceLocal(renderVertices.data(),
        sizeof(JellyVertex) * renderVertices.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        _vertexBuffer);

    uploadDeviceLocal(renderIndices.data(),
        sizeof(uint32_t) * renderIndices.size(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        _indexBuffer);

    uploadDeviceLocal(renderTriangles.data(),
        sizeof(MSTriangle) * renderTriangles.size(),
        0,
        _renderTrianglesBuffer);

    uploadDeviceLocal(renderVertexTriRanges.data(),
        sizeof(MSParticleTriRange) * renderVertexTriRanges.size(),
        0,
        _renderVertexTriRangeBuffer);

    VkDescriptorBufferInfo rvInfo    = _vertexBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo triInfo   = _renderTrianglesBuffer->getDescriptorInfo();
    VkDescriptorBufferInfo rangeInfo = _renderVertexTriRangeBuffer->getDescriptorInfo();

    // One descriptor set per physics ping-pong state, so dispatch reads
    // whichever physics buffer was just written.
    for (int i = 0; i < 2; ++i)
    {
        VkDescriptorBufferInfo physInfo{ getParticleBuffer(i), 0, VK_WHOLE_SIZE };

        _preRenderDescriptorSets[i] = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
            Descriptor(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, physInfo),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, rangeInfo),
            Descriptor(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, triInfo),
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1, rvInfo),
        });
    }

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(RenderNormalsPC);

    _preRenderPipeline = std::make_unique<ComputePipeline>(
        _ctx,
        AssetPath::getInstance()->get("spv/jelly/jelly_pre_render_comp.spv"),
        std::vector<VkDescriptorSetLayout>{ _preRenderDescriptorSets[0]->getDescriptorSetLayout() },
        std::vector<VkPushConstantRange>{ pcRange }
    );
}


void JellySimulation::dispatchPreRender(VkCommandBuffer cmd)
{
    _preRenderPipeline->bind(cmd);

    VkDescriptorSet ds = _preRenderDescriptorSets[_currentIn]->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        _preRenderPipeline->getPipelineLayout(), 0, 1, &ds, 0, nullptr);

    RenderNormalsPC pc{ _numRenderVertices };
    vkCmdPushConstants(cmd, _preRenderPipeline->getPipelineLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RenderNormalsPC), &pc);

    uint32_t groups = (_numRenderVertices + 63) / 64;
    vkCmdDispatch(cmd, groups, 1, 1);
}
