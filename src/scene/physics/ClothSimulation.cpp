#include "ClothSimulation.h"
#include "core/AssetPath.h"

enum class SpringType { Structural, Shear, Bend };

struct CpuSpring {
    int32_t a, b;
    float   restLength, ratio;
    SpringType type;
};

ClothSimulation::ClothSimulation(std::shared_ptr<VulkanContext> ctx,
                                 int rows, int cols, float spacing,
                                 ClothPinMode pinMode,
                                 glm::mat4 initialTransform)
    : MassSpringSimulation(std::move(ctx))
{
    auto idx = [&](int r, int c) { return r * cols + c; };

    // Build particle grid
    std::vector<MSParticle> particles(rows * cols);
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
            particles[i].position     = pos;
            particles[i].prevPosition = pos;
            particles[i].normal       = glm::vec3(0.f, 0.f, 1.f);
            particles[i].uv           = glm::vec2((float)c / (cols - 1),
                                                   1.0f - (float)r / (rows - 1));
            bool pinned = (pinMode == ClothPinMode::LeftColumn && c == 0) ||
                          (pinMode == ClothPinMode::TopEdge   && r == rows - 1);
            particles[i].invMass = pinned ? 0.f : 1.f;
        }
    }

    // Build springs
    std::vector<CpuSpring> cpuSprings;
    auto addSpring = [&](int a, int b, float ratio, SpringType type)
    {
        float rest = glm::length(particles[a].position - particles[b].position);
        cpuSprings.push_back({a, b, rest, ratio, type});
    };

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            if (c + 1 < cols) addSpring(idx(r, c), idx(r,   c+1), 1.0f, SpringType::Structural);
            if (r + 1 < rows) addSpring(idx(r, c), idx(r+1, c  ), 1.0f, SpringType::Structural);
            if (r + 1 < rows && c + 1 < cols) addSpring(idx(r, c), idx(r+1, c+1), 0.6f, SpringType::Shear);
            if (r + 1 < rows && c - 1 >= 0  ) addSpring(idx(r, c), idx(r+1, c-1), 0.6f, SpringType::Shear);
            if (c + 2 < cols) addSpring(idx(r, c), idx(r,   c+2), 0.4f, SpringType::Bend);
            if (r + 2 < rows) addSpring(idx(r, c), idx(r+2, c  ), 0.4f, SpringType::Bend);
        }
    }

    // Build index buffer
    std::vector<uint32_t> indices;
    for (int r = 0; r < rows - 1; ++r)
    {
        for (int c = 0; c < cols - 1; ++c)
        {
            uint32_t tl = idx(r,   c),   tr = idx(r,   c+1);
            uint32_t bl = idx(r+1, c),   br = idx(r+1, c+1);
            indices.insert(indices.end(), {tl, bl, tr});
            indices.insert(indices.end(), {tr, bl, br});
        }
    }

    // Build directed spring adjacency list
    uint32_t numParticles = static_cast<uint32_t>(particles.size());
    std::vector<MSParticleMetadata> metadata(numParticles);
    std::vector<MSSpring>           directedSprings;
    directedSprings.reserve(cpuSprings.size() * 2);

    for (uint32_t p = 0; p < numParticles; ++p)
    {
        metadata[p].springStartIndex = static_cast<uint32_t>(directedSprings.size());
        for (const auto& s : cpuSprings)
        {
            if (s.a == (int32_t)p)
                directedSprings.push_back({ s.b, s.restLength, s.ratio });
            else if (s.b == (int32_t)p)
                directedSprings.push_back({ s.a, s.restLength, s.ratio });
        }
        metadata[p].springCount = static_cast<uint32_t>(directedSprings.size()) - metadata[p].springStartIndex;
    }

    // Apply initial transform to particle positions and normals
    glm::mat3 normalMatrix = glm::mat3(initialTransform);
    for (auto& p : particles) {
        p.position     = glm::vec3(initialTransform * glm::vec4(p.position, 1.f));
        p.prevPosition = p.position;
        p.normal       = glm::normalize(normalMatrix * p.normal);
    }

    init(particles, metadata, directedSprings, indices,
         AssetPath::getInstance()->get("spv/compute/mass_spring_integrate_comp.spv"));
}
