#include "JellySimulation.h"
#include "core/AssetPath.h"

JellySimulation::JellySimulation(std::shared_ptr<VulkanContext> ctx,
                                 int resX, int resY, int resZ,
                                 float spacing,
                                 glm::vec3 center)
    : MassSpringSimulation(std::move(ctx))
{
    auto idx = [&](int x, int y, int z) { return x + y * resX + z * resX * resY; };
    int numParticles = resX * resY * resZ;

    // ---- Particles ----
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
        particles[i].position     = pos;
        particles[i].prevPosition = pos;
        particles[i].normal       = glm::vec3(0.f, 1.f, 0.f);
        particles[i].uv           = glm::vec2((float)x / (resX - 1), (float)y / (resY - 1));
        particles[i].invMass      = 1.f;
    }

    // ---- Springs ----
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

    struct CpuSpring { int32_t a, b; float restLength, ratio; };
    std::vector<CpuSpring> cpuSprings;

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

    // ---- Surface index buffer (outward-facing winding per face) ----
    std::vector<uint32_t> indices;
    auto addQuad = [&](int a, int b, int c, int d) {
        indices.insert(indices.end(), {(uint32_t)a, (uint32_t)b, (uint32_t)c});
        indices.insert(indices.end(), {(uint32_t)a, (uint32_t)c, (uint32_t)d});
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

    // ---- Directed spring adjacency list ----
    std::vector<MSParticleMetadata> metadata(numParticles);
    std::vector<MSSpring>           directedSprings;
    directedSprings.reserve(cpuSprings.size() * 2);

    for (int p = 0; p < numParticles; ++p)
    {
        metadata[p].springStartIndex = static_cast<uint32_t>(directedSprings.size());
        for (auto& s : cpuSprings)
        {
            if (s.a == p) directedSprings.push_back({s.b, s.restLength, s.ratio});
            else if (s.b == p) directedSprings.push_back({s.a, s.restLength, s.ratio});
        }
        metadata[p].springCount = static_cast<uint32_t>(directedSprings.size()) - metadata[p].springStartIndex;
    }

    init(particles, metadata, directedSprings, indices,
         AssetPath::getInstance()->get("spv/compute/mass_spring_integrate_comp.spv"));
}
