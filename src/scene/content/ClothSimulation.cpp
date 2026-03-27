#include "ClothSimulation.h"

void ClothSimulation::init(int rows, int cols, float spacing)
{
    _rows = rows;
    _cols = cols;
    _particles.clear();
    _springs.clear();
    _indices.clear();

    // Build particle grid — centred at origin, lying flat in XZ plane
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            ClothParticle p;
            p.position = glm::vec3(
                c * spacing - (cols - 1) * spacing * 0.5f,
                r * spacing - (rows - 1) * spacing * 0.5f,
                0.f
            );
            p.prevPosition = p.position;
            p.force        = glm::vec3(0.f);
            p.mass         = 1.0f;
            p.pinned       = (c == 0 ); // pin 1st column (flag!)  && (r==0 || r==rows-1)
            _particles.push_back(p);
        }
    }

    auto addSpring = [&](int a, int b, float ratio, SpringType type)
    {
        float rest = glm::length(_particles[a].position - _particles[b].position);
        _springs.push_back({a, b, rest, ratio, type});
    };

    for (int r = 0; r < rows; ++r)
    {gi
        for (int c = 0; c < cols; ++c)
        {
            // Structural (horizontal + vertical)
            if (c + 1 < cols) addSpring(idx(r, c), idx(r,   c+1), 1.0f, SpringType::Structural);
            if (r + 1 < rows) addSpring(idx(r, c), idx(r+1, c  ), 1.0f, SpringType::Structural);

            // Shear (diagonals)
            if (r + 1 < rows && c + 1 < cols) addSpring(idx(r, c), idx(r+1, c+1), 0.6f, SpringType::Shear);
            if (r + 1 < rows && c - 1 >= 0  ) addSpring(idx(r, c), idx(r+1, c-1), 0.6f, SpringType::Shear);

            // Bend (every-other neighbour, resists folding)
            if (c + 2 < cols) addSpring(idx(r, c), idx(r,   c+2), 0.4f, SpringType::Bend);
            if (r + 2 < rows) addSpring(idx(r, c), idx(r+2, c  ), 0.4f, SpringType::Bend);
        }
    }

    // Build triangle index buffer (fixed — topology never changes)
    for (int r = 0; r < rows - 1; ++r)
    {
        for (int c = 0; c < cols - 1; ++c)
        {
            uint32_t tl = idx(r,   c);
            uint32_t tr = idx(r,   c+1);
            uint32_t bl = idx(r+1, c);
            uint32_t br = idx(r+1, c+1);
            // Two triangles per quad
            _indices.insert(_indices.end(), {tl, bl, tr});
            _indices.insert(_indices.end(), {tr, bl, br});
        }
    }
}

void ClothSimulation::step(float dt)
{
    _time += dt;
    float subDt = dt / substeps;
    for (int i = 0; i < substeps; ++i)
    {
        applyForces(subDt);
        integrate(subDt);
    }
}

void ClothSimulation::applyForces(float dt)
{
    const glm::vec3 gravity(0.f, -9.8f, 0.f);

    // Apply Gravity
    for (auto& p : _particles)
        p.force = (!p.pinned && gravityEnabled) ? gravity * p.mass : glm::vec3(0.f); // F = ma

    // Apply aerodynamic wind force per triangle
    if (windEnabled)
    {
        glm::vec3 dir = glm::length(windDirection) > 1e-6f
                      ? glm::normalize(windDirection)
                      : glm::vec3(0.f, 0.f, 1.f);

        // Smooth value noise: hash integer keyframes, smoothstep between them.
        // Aperiodic and stateless — far more natural than a single sine wave.
        auto hash01 = [](int32_t n) -> float {
            uint32_t u = static_cast<uint32_t>(n);
            u = (u ^ 2747636419u) * 2654435769u;
            u ^= u >> 16u;
            u *= 2654435769u;
            return static_cast<float>(u >> 8) / static_cast<float>(1u << 24);
        };
        float gt  = _time * 0.45f;          // ~2.2 s between gust peaks on average
        int   gti = static_cast<int>(std::floor(gt));
        float f   = gt - static_cast<float>(gti);
        float s   = f * f * (3.f - 2.f * f);   // smoothstep
        float gust = windStrength * (0.55f + 0.9f * glm::mix(hash01(gti), hash01(gti + 1), s));

        for (size_t i = 0; i + 2 < _indices.size(); i += 3)
        {
            ClothParticle& p0 = _particles[_indices[i    ]];
            ClothParticle& p1 = _particles[_indices[i + 1]];
            ClothParticle& p2 = _particles[_indices[i + 2]];

            // Face normal and area from cross product
            glm::vec3 edge1 = p1.position - p0.position;
            glm::vec3 edge2 = p2.position - p0.position;
            glm::vec3 cross = glm::cross(edge1, edge2);
            float area = 0.5f * glm::length(cross);
            if (area < 1e-8f) continue;
            glm::vec3 n = cross / (2.f * area);

            // Average face velocity via Verlet: v ≈ (pos - prevPos) / dt
            glm::vec3 vFace = ((p0.position - p0.prevPosition)
                             + (p1.position - p1.prevPosition)
                             + (p2.position - p2.prevPosition)) / (3.f * dt);

            // Turbulence keyed on triangle centroid
            glm::vec3 c = (p0.position + p1.position + p2.position) / 3.f;
            float tx = std::sin(_time * 2.3f + c.x * 0.7f) + std::sin(_time * 1.1f + c.y * 1.3f) * 0.5f;
            float ty = std::cos(_time * 1.7f + c.y * 0.9f) + std::cos(_time * 3.1f + c.x * 0.4f) * 0.4f;
            float tz = std::sin(_time * 2.7f + c.x * 0.5f + c.y * 0.6f);
            glm::vec3 windVel = gust * dir + glm::vec3(tx, ty, tz) * windTurbulence;

            // F = Cd * A * (v_rel · n) * n  — zero when face is edge-on to wind
            glm::vec3 vRel  = windVel - vFace;
            glm::vec3 force = windDragCoeff * area * glm::dot(vRel, n) * n;

            // Distribute equally to the 3 vertices
            glm::vec3 f3 = force / 3.f;
            if (!p0.pinned) p0.force += f3;
            if (!p1.pinned) p1.force += f3;
            if (!p2.pinned) p2.force += f3;
        }
    }

    // Apply Hooke's law
    for (const auto& s : _springs)
    {
        if (s.type == SpringType::Shear && !shearEnabled) continue;
        if (s.type == SpringType::Bend  && !bendEnabled)  continue;

        ClothParticle& pa = _particles[s.a];
        ClothParticle& pb = _particles[s.b];

        glm::vec3 delta = pb.position - pa.position;
        float len = glm::length(delta);
        if (len < 1e-6f) continue;
        glm::vec3 dir = delta / len;

        // Hooke: F = k * (len - rest)
        glm::vec3 force = s.ratio * stiffness * (len - s.restLength) * dir;

        // Spring damping: resist rate of extension/compression along spring axis
        // velocity from Verlet: v ≈ (pos - prevPos) / dt
        glm::vec3 relVel = (pb.position - pb.prevPosition) - (pa.position - pa.prevPosition);
        force += springDamping * glm::dot(relVel, dir) * dir;

        if (!pa.pinned) pa.force += force;
        if (!pb.pinned) pb.force -= force;
    }
}

void ClothSimulation::integrate(float dt)
{
    for (auto& p : _particles)
    {
        if (p.pinned) continue;
        glm::vec3 vel    = (p.position - p.prevPosition) * damping;
        glm::vec3 newPos = p.position + vel + (p.force / p.mass) * dt * dt;
        p.prevPosition   = p.position;
        p.position       = newPos;
    }
}
