#include "MeshFactory.h"
#include "Vertex.h"
#include "HostMesh.h"
#include "Vertex.h"

namespace MeshFactory {

    HostMesh createSphereMesh(float radius, int segments, int rings, bool skySphere)
    {
        HostMesh mesh;

        // Generate vertices and texture coordinates
        for (int y = 0; y <= rings; ++y) {
            float v = (float)y / rings;
            float theta = v * glm::pi<float>(); // from 0 to PI

            for (int x = 0; x <= segments; ++x) {
                float u = (float)x / segments;
                float phi = u * 2.0f * glm::pi<float>(); // from 0 to 2PI

                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                float px = radius * sinTheta * cosPhi;
                float py = radius * sinTheta * sinPhi;
                float pz = radius * cosTheta;

                Vertex vert;
                vert.pos = { px, py, pz };
                vert.color = glm::vec4(1.f);
                vert.texCoord = { u, v };
                vert.normal = glm::normalize(vert.pos);
                vert.tangent = glm::normalize(glm::vec3(-sinPhi, cosPhi, 0.0)); // Tangent (u direction)

                // Add vertex
                mesh.vertices.push_back(vert);
            }
        }

        // Generate indices
        for (int y = 0; y < rings; ++y) {
            for (int x = 0; x < segments; ++x) {
                int i0 = y * (segments + 1) + x;
                int i1 = i0 + 1;
                int i2 = i0 + (segments + 1);
                int i3 = i2 + 1;

                if (skySphere) {
                    // Triangle 1
                    mesh.indices.push_back(i0);
                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i2);

                    // Triangle 2
                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i3);
                    mesh.indices.push_back(i2);
                }
                else
                {
                    // Triangle 1
                    mesh.indices.push_back(i0);
                    mesh.indices.push_back(i2);
                    mesh.indices.push_back(i1);

                    // Triangle 2
                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i2);
                    mesh.indices.push_back(i3);
                }
            }
        }

        return mesh;
    }


    // HostMesh createBoxMesh(float width, float height, float depth) {
    //     HostMesh mesh;

    //     float hw = width / 2.0f;   // half width
    //     float hh = height / 2.0f;  // half height
    //     float hd = depth / 2.0f;   // half depth

    //     // Each face needs its own vertices because normals differ per face
    //     // Front face (+Z)
    //     mesh.vertices.push_back({{ -hw, -hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw, -hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw,  hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw,  hh,  hd }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f});

    //     // Back face (-Z)
    //     mesh.vertices.push_back({{  hw, -hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw, -hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw,  hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw,  hh, -hd }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f});

    //     // Left face (-X)
    //     mesh.vertices.push_back({{ -hw, -hh, -hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw, -hh,  hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw,  hh,  hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw,  hh, -hd }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f});

    //     // Right face (+X)
    //     mesh.vertices.push_back({{  hw, -hh,  hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw, -hh, -hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw,  hh, -hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw,  hh,  hd }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f});

    //     // Top face (+Y)
    //     mesh.vertices.push_back({{ -hw,  hh,  hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw,  hh,  hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw,  hh, -hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw,  hh, -hd }, 0.0f, { 0.0f, 1.0f, 0.0f }, 0.0f});

    //     // Bottom face (-Y)
    //     mesh.vertices.push_back({{ -hw, -hh, -hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw, -hh, -hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{  hw, -hh,  hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});
    //     mesh.vertices.push_back({{ -hw, -hh,  hd }, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f});

    //     // Indices - each face is 2 triangles (counter-clockwise winding)
    //     for (uint32_t i = 0; i < 6; i++) {
    //         uint32_t base = i * 4;
    //         // First triangle
    //         mesh.indices.push_back(base + 0);
    //         mesh.indices.push_back(base + 1);
    //         mesh.indices.push_back(base + 2);
    //         // Second triangle
    //         mesh.indices.push_back(base + 0);
    //         mesh.indices.push_back(base + 2);
    //         mesh.indices.push_back(base + 3);
    //     }

    //     return mesh;
    // }


    // HostMesh createDoughnutMesh(float innerRadius, float outerRadius, int segments, int tubeSegments) {
    //     HostMesh mesh;

    //     float segmentStep = 2.0f * glm::pi<float>() / static_cast<float>(segments);
    //     float tubeStep = 2.0f * glm::pi<float>() / static_cast<float>(tubeSegments);
    //     float tubeRadius = (outerRadius - innerRadius) / 2.0f;
    //     float centerRadius = innerRadius + tubeRadius;

    //     for (int i = 0; i <= segments; ++i) {
    //         float segmentAngle = i * segmentStep;
    //         glm::vec3 segmentCenter = {
    //             centerRadius * std::cos(segmentAngle),
    //             0.0f,
    //             centerRadius * std::sin(segmentAngle)
    //         };

    //         for (int j = 0; j <= tubeSegments; ++j) {
    //             float tubeAngle = j * tubeStep;
    //             glm::vec3 offset = {
    //                 tubeRadius * std::cos(tubeAngle) * std::cos(segmentAngle),
    //                 tubeRadius * std::sin(tubeAngle),
    //                 tubeRadius * std::cos(tubeAngle) * std::sin(segmentAngle)
    //             };

    //             Vertex vert;
    //             vert.pos = segmentCenter + offset;
    //             vert.normal = glm::normalize(offset);
    //             mesh.vertices.push_back(vert);
    //         }
    //     }

    //     // Generate indices
    //     for (int i = 0; i < segments; ++i) {
    //         for (int j = 0; j < tubeSegments; ++j) {
    //             int current = i * (tubeSegments + 1) + j;
    //             int next = (i + 1) * (tubeSegments + 1) + j;

    //             mesh.indices.push_back(current);
    //             mesh.indices.push_back(next);
    //             mesh.indices.push_back(current + 1);

    //             mesh.indices.push_back(current + 1);
    //             mesh.indices.push_back(next);
    //             mesh.indices.push_back(next + 1);
    //         }
    //     }

    //     return mesh;
    // }

    // HostMesh createConeMesh(float baseRadius, float height, int segments, bool capped) {
    //     HostMesh mesh;

    //     float angleStep = 2.0f * glm::pi<float>() / static_cast<float>(segments);

    //     // Apex vertex - normal points upward
    //     Vertex apex;
    //     apex.pos = { 0.0f, height, 0.0f };
    //     apex.normal = { 0.0f, 1.0f, 0.0f };
    //     mesh.vertices.push_back(apex);

    //     // Base circle vertices with smooth normals
    //     for (int i = 0; i <= segments; ++i) {
    //         float angle = i * angleStep;
    //         float x = baseRadius * std::cos(angle);
    //         float z = baseRadius * std::sin(angle);

    //         Vertex baseVertex;
    //         baseVertex.pos = { x, 0.0f, z };
    //         baseVertex.normal = glm::normalize(glm::vec3(x, baseRadius / height, z));
    //         mesh.vertices.push_back(baseVertex);
    //     }

    //     // Side triangles
    //     for (int i = 1; i <= segments; ++i) {
    //         mesh.indices.push_back(0); // Apex
    //         mesh.indices.push_back(i);
    //         mesh.indices.push_back(i + 1);
    //     }

    //     // Base cap (if needed)
    //     if (capped) {
    //         uint32_t centerIndex = mesh.vertices.size();
    //         mesh.vertices.push_back({{0.0f, 0.0f, 0.0f}, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f});

    //         for (int i = 0; i < segments; ++i) {
    //             float angle1 = i * angleStep;
    //             float angle2 = (i + 1) * angleStep;

    //             glm::vec3 base1 = { baseRadius * std::cos(angle1), 0.0f, baseRadius * std::sin(angle1) };
    //             glm::vec3 base2 = { baseRadius * std::cos(angle2), 0.0f, baseRadius * std::sin(angle2) };

    //             uint32_t baseIdx = mesh.vertices.size();
    //             mesh.vertices.push_back({base1, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f});
    //             mesh.vertices.push_back({base2, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f});

    //             mesh.indices.push_back(centerIndex);
    //             mesh.indices.push_back(baseIdx + 1);
    //             mesh.indices.push_back(baseIdx + 0);
    //         }
    //     }

    //     return mesh;
    // }

    // HostMesh createCylinderMesh(float radius, float height, int segments, bool capped) {
    //     HostMesh mesh;

    //     float halfHeight = height * 0.5f;
    //     float angleStep = 2.0f * glm::pi<float>() / float(segments);

    //     // Side vertices
    //     for (int i = 0; i <= segments; ++i)
    //     {
    //         float angle = float(i) * angleStep;
    //         float cosA = std::cos(angle);
    //         float sinA = std::sin(angle);

    //         glm::vec3 n = glm::normalize(glm::vec3(cosA, 0, sinA));

    //         // top vertex
    //         mesh.vertices.push_back({
    //             glm::vec3(cosA * radius,  halfHeight, sinA * radius),
    //             0.0f,
    //             n,
    //             0.0f
    //         });

    //         // bottom vertex
    //         mesh.vertices.push_back({
    //             glm::vec3(cosA * radius, -halfHeight, sinA * radius),
    //             0.0f,
    //             n,
    //             0.0f
    //         });
    //     }

    //     // Side indices
    //     for (int i = 0; i < segments; ++i)
    //     {
    //         int top1    = 2*i;
    //         int bottom1 = top1 + 1;

    //         int top2    = 2*(i+1);
    //         int bottom2 = top2 + 1;

    //         // triangle 1
    //         mesh.indices.push_back(top1);
    //         mesh.indices.push_back(bottom1);
    //         mesh.indices.push_back(top2);

    //         // triangle 2
    //         mesh.indices.push_back(bottom1);
    //         mesh.indices.push_back(bottom2);
    //         mesh.indices.push_back(top2);
    //     }

    //     if (capped)
    //     {
    //         // Top cap
    //         uint32_t topCenter = mesh.vertices.size();
    //         mesh.vertices.push_back({ {0, halfHeight, 0}, 0.0f, {0,1,0}, 0.0f });

    //         uint32_t topStart = mesh.vertices.size();

    //         for (int i = 0; i <= segments; ++i)
    //         {
    //             float angle = float(i) * angleStep;
    //             float x = std::cos(angle) * radius;
    //             float z = std::sin(angle) * radius;

    //             mesh.vertices.push_back({ {x, halfHeight, z}, 0.0f, {0,1,0}, 0.0f });
    //         }

    //         for (int i = 0; i < segments; ++i)
    //         {
    //             mesh.indices.push_back(topCenter);
    //             mesh.indices.push_back(topStart + i);
    //             mesh.indices.push_back(topStart + i + 1);
    //         }

    //         // Bottom cap
    //         uint32_t bottomCenter = mesh.vertices.size();
    //         mesh.vertices.push_back({ {0,-halfHeight,0}, 0.0f, {0,-1,0}, 0.0f });

    //         uint32_t bottomStart = mesh.vertices.size();

    //         for (int i = 0; i <= segments; ++i)
    //         {
    //             float angle = float(i) * angleStep;
    //             float x = std::cos(angle) * radius;
    //             float z = std::sin(angle) * radius;

    //             mesh.vertices.push_back({ {x,-halfHeight,z}, 0.0f, {0,-1,0}, 0.0f });
    //         }

    //         for (int i = 0; i < segments; ++i)
    //         {
    //             mesh.indices.push_back(bottomCenter);
    //             mesh.indices.push_back(bottomStart + i + 1);
    //             mesh.indices.push_back(bottomStart + i);
    //         }
    //     }

    //     return mesh;
    // }

    // HostMesh createPrismMesh(float radius, float height, int sides, bool capped) {
    //     HostMesh mesh;

    //     float angleStep = 2.f * glm::pi<float>() / float(sides);
    //     float halfHeight = height * 0.5f;

    //     // SIDE FACES (flat normals)
    //     for (int i = 0; i < sides; ++i)
    //     {
    //         int next = (i + 1) % sides;

    //         float a1 = i * angleStep;
    //         float a2 = next * angleStep;

    //         glm::vec3 b1 = { radius * cos(a1), -halfHeight, radius * sin(a1) };
    //         glm::vec3 t1 = { radius * cos(a1),  halfHeight, radius * sin(a1) };

    //         glm::vec3 b2 = { radius * cos(a2), -halfHeight, radius * sin(a2) };
    //         glm::vec3 t2 = { radius * cos(a2),  halfHeight, radius * sin(a2) };

    //         glm::vec3 normal = glm::normalize(glm::cross(t1 - b1, b2 - b1));

    //         uint32_t i0 = mesh.vertices.size();
    //         mesh.vertices.push_back({ b1, 0, normal, 0 });
    //         mesh.vertices.push_back({ t1, 0, normal, 0 });
    //         mesh.vertices.push_back({ b2, 0, normal, 0 });
    //         mesh.vertices.push_back({ t2, 0, normal, 0 });

    //         mesh.indices.insert(mesh.indices.end(),
    //         {
    //             i0,     i0 + 1, i0 + 2,
    //             i0 + 1, i0 + 3, i0 + 2
    //         });
    //     }

    //     // CAPS (optional)
    //     if (capped)
    //     {
    //         // top center
    //         uint32_t topCenter = mesh.vertices.size();
    //         mesh.vertices.push_back({ {0, halfHeight, 0}, 0, {0,1,0}, 0 });

    //         // bottom center
    //         uint32_t bottomCenter = mesh.vertices.size();
    //         mesh.vertices.push_back({ {0,-halfHeight, 0}, 0, {0,-1,0}, 0 });

    //         // top cap ring
    //         uint32_t topStart = mesh.vertices.size();
    //         for (int i = 0; i <= sides; ++i)
    //         {
    //             float a = i * angleStep;
    //             mesh.vertices.push_back({
    //                 { radius*cos(a), halfHeight, radius*sin(a) },
    //                 0, {0,1,0}, 0
    //             });
    //         }

    //         // top cap triangles
    //         for (int i = 0; i < sides; ++i)
    //         {
    //             mesh.indices.push_back(topCenter);
    //             mesh.indices.push_back(topStart + i);
    //             mesh.indices.push_back(topStart + i + 1);
    //         }

    //         // bottom cap ring
    //         uint32_t bottomStart = mesh.vertices.size();
    //         for (int i = 0; i <= sides; ++i)
    //         {
    //             float a = i * angleStep;
    //             mesh.vertices.push_back({
    //                 { radius*cos(a), -halfHeight, radius*sin(a) },
    //                 0, {0,-1,0}, 0
    //             });
    //         }

    //         // bottom cap triangles
    //         for (int i = 0; i < sides; ++i)
    //         {
    //             mesh.indices.push_back(bottomCenter);
    //             mesh.indices.push_back(bottomStart + i + 1);
    //             mesh.indices.push_back(bottomStart + i);
    //         }
    //     }

    //     return mesh;
    // }

    // HostMesh createIcosahedronMesh(float radius) {
    //     HostMesh mesh;

    //     const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    //     // Base vertex positions
    //     std::vector<glm::vec3> basePositions = {
    //         {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
    //         { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
    //         { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
    //     };

    //     // Normalize and scale base positions
    //     for (auto& pos : basePositions) {
    //         pos = glm::normalize(pos) * radius;
    //     }

    //     // Triangle indices (20 faces)
    //     std::vector<std::array<uint32_t, 3>> faces = {
    //         // 5 faces around vertex 0
    //         {0,11,5}, {0,5,1}, {0,1,7}, {0,7,10}, {0,10,11},
    //         // 5 adjacent faces
    //         {1,5,9}, {5,11,4}, {11,10,2}, {10,7,6}, {7,1,8},
    //         // 5 faces around vertex 3
    //         {3,9,4}, {3,4,2}, {3,2,6}, {3,6,8}, {3,8,9},
    //         // 5 adjacent faces
    //         {4,9,5}, {2,4,11}, {6,2,10}, {8,6,7}, {9,8,1}
    //     };

    //     // Create vertices with flat face normals
    //     for (const auto& face : faces) {
    //         glm::vec3 v0 = basePositions[face[0]];
    //         glm::vec3 v1 = basePositions[face[1]];
    //         glm::vec3 v2 = basePositions[face[2]];

    //         // Calculate flat face normal
    //         glm::vec3 edge1 = v1 - v0;
    //         glm::vec3 edge2 = v2 - v0;
    //         glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

    //         // Add three vertices with the same face normal
    //         uint32_t baseIdx = mesh.vertices.size();
    //         mesh.vertices.push_back({v0, 0.0f, normal, 0.0f});
    //         mesh.vertices.push_back({v1, 0.0f, normal, 0.0f});
    //         mesh.vertices.push_back({v2, 0.0f, normal, 0.0f});

    //         // Add indices for this triangle
    //         mesh.indices.push_back(baseIdx + 0);
    //         mesh.indices.push_back(baseIdx + 1);
    //         mesh.indices.push_back(baseIdx + 2);
    //     }

    //     return mesh;
    // }

    // HostMesh createRhombusMesh(float edgeLength, float height)
    // {
    //     HostMesh mesh;

    //     float halfEdge = edgeLength * 0.5f;
    //     float halfHeight = height * 0.5f;

    //     // Top apex
    //     glm::vec3 topApex = { 0.0f, halfHeight, 0.0f };

    //     // Bottom apex
    //     glm::vec3 bottomApex = { 0.0f, -halfHeight, 0.0f };

    //     // Middle square vertices (in XZ plane at y=0)
    //     glm::vec3 v0 = { -halfEdge, 0.0f, -halfEdge };
    //     glm::vec3 v1 = {  halfEdge, 0.0f, -halfEdge };
    //     glm::vec3 v2 = {  halfEdge, 0.0f,  halfEdge };
    //     glm::vec3 v3 = { -halfEdge, 0.0f,  halfEdge };

    //     // Helper to add a triangle face with calculated normal
    //     auto addTriangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
    //         glm::vec3 edge1 = b - a;
    //         glm::vec3 edge2 = c - a;
    //         glm::vec3 normal = -glm::normalize(glm::cross(edge1, edge2));

    //         uint32_t baseIdx = mesh.vertices.size();
    //         mesh.vertices.push_back({a, 0.0f, normal, 0.0f});
    //         mesh.vertices.push_back({b, 0.0f, normal, 0.0f});
    //         mesh.vertices.push_back({c, 0.0f, normal, 0.0f});

    //         mesh.indices.push_back(baseIdx + 0);
    //         mesh.indices.push_back(baseIdx + 1);
    //         mesh.indices.push_back(baseIdx + 2);
    //     };

    //     // Top pyramid faces (4 triangles)
    //     addTriangle(topApex, v0, v1);
    //     addTriangle(topApex, v1, v2);
    //     addTriangle(topApex, v2, v3);
    //     addTriangle(topApex, v3, v0);

    //     // Bottom pyramid faces (4 triangles)
    //     addTriangle(bottomApex, v1, v0);
    //     addTriangle(bottomApex, v2, v1);
    //     addTriangle(bottomApex, v3, v2);
    //     addTriangle(bottomApex, v0, v3);

    //     return mesh;
    // }

    HostMesh createAnnulusMesh(float innerRadius, float outerRadius, int segments)
    {
        HostMesh mesh;

        float angleStep = 2.0f * glm::pi<float>() / static_cast<float>(segments);

        for (uint32_t i = 0; i <= segments; ++i) {

            float angle = i * angleStep;
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            // Outer vertex (XY plane so Planet's Rx(-90°) rotates it flat into XZ)
            Vertex outerVertex;
            outerVertex.pos = { cosA * outerRadius, sinA * outerRadius, 0.0f };
            outerVertex.color = glm::vec4(1.f);
            outerVertex.texCoord = { 1.0f, static_cast<float>(i) / segments };
            outerVertex.normal = glm::vec3(0, 0, 1);
            outerVertex.tangent = glm::vec3(-sinA, cosA, 0.0f);
            mesh.vertices.push_back(outerVertex);

            // Inner vertex
            Vertex innerVertex;
            innerVertex.pos = { cosA * innerRadius, sinA * innerRadius, 0.0f };
            innerVertex.color = glm::vec4(1.f);
            innerVertex.texCoord = { 0.0f, static_cast<float>(i) / segments };
            innerVertex.normal = glm::vec3(0, 0, 1);
            innerVertex.tangent = glm::vec3(-sinA, cosA, 0.0f);
            mesh.vertices.push_back(innerVertex);

        }

        // Generate indices for both sides of the annulus
        for (uint32_t i = 0; i < segments; ++i) {
            uint32_t outerIndex = i * 2;
            uint32_t innerIndex = outerIndex + 1;
            uint32_t nextOuterIndex = ((i + 1) % segments) * 2;
            uint32_t nextInnerIndex = nextOuterIndex + 1;

            // Triangle 1
            mesh.indices.push_back(outerIndex);
            mesh.indices.push_back(innerIndex);
            mesh.indices.push_back(nextOuterIndex);

            // Triangle 2
            mesh.indices.push_back(innerIndex);
            mesh.indices.push_back(nextInnerIndex);
            mesh.indices.push_back(nextOuterIndex);

            mesh.indices.push_back(outerIndex);
            mesh.indices.push_back(nextOuterIndex);
            mesh.indices.push_back(innerIndex);

            mesh.indices.push_back(innerIndex);
            mesh.indices.push_back(nextOuterIndex);
            mesh.indices.push_back(nextInnerIndex);

        }

        return mesh;
    }

    HostMesh createQuadMesh(float width, float height, bool twoSided)
    {
        HostMesh mesh;

        Vertex v0;
        v0.pos = { -width / 2, 0.0f, -height / 2 };
        v0.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        v0.texCoord = { 0.0f, 0.0f };
        v0.normal = { 0.0f, 1.0f, 0.0f };
        v0.tangent = { 1.0f, 0.0f, 0.0f };

        Vertex v1;
        v1.pos = { width / 2, 0.0f, -height / 2 };
        v1.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        v1.texCoord = { 1.0f, 0.0f };
        v1.normal = { 0.0f, 1.0f, 0.0f };
        v1.tangent = { 1.0f, 0.0f, 0.0f };

        Vertex v2;
        v2.pos = { width / 2, 0.0f, height / 2 };
        v2.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        v2.texCoord = { 1.0f, 1.0f };
        v2.normal = { 0.0f, 1.0f, 0.0f };
        v2.tangent = { 1.0f, 0.0f, 0.0f };

        Vertex v3;
        v3.pos = { -width / 2, 0.0f, height / 2 };
        v3.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        v3.texCoord = { 0.0f, 1.0f };
        v3.normal = { 0.0f, 1.0f, 0.0f };
        v3.tangent = { 1.0f, 0.0f, 0.0f };

        mesh.vertices.push_back(v0);
        mesh.vertices.push_back(v1);
        mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3);

        mesh.indices.push_back(0);
        mesh.indices.push_back(1);
        mesh.indices.push_back(2);

        mesh.indices.push_back(0);
        mesh.indices.push_back(2);
        mesh.indices.push_back(3);

        if(twoSided) {
            mesh.indices.push_back(0);
            mesh.indices.push_back(2);
            mesh.indices.push_back(1);
    
            mesh.indices.push_back(0);
            mesh.indices.push_back(3);
            mesh.indices.push_back(2);
        }

        return mesh;
    }

    HostMesh createCubeMesh(float width, float height, float depth)
    {
        HostMesh mesh;

        // Define the 8 vertices of the cube
        std::array<glm::vec3, 8> vertices = {
            glm::vec3(-width / 2, -height / 2, -depth / 2),
            glm::vec3(width / 2, -height / 2, -depth / 2),
            glm::vec3(width / 2, height / 2, -depth / 2),
            glm::vec3(-width / 2, height / 2, -depth / 2),
            glm::vec3(-width / 2, -height / 2, depth / 2),
            glm::vec3(width / 2, -height / 2, depth / 2),
            glm::vec3(width / 2, height / 2, depth / 2),
            glm::vec3(-width / 2, height / 2, depth / 2)
        };

        // Define the indices for the cube's faces
        std::array<uint32_t, 36> indices = {
            // Front face
            0, 2, 1,
            0, 3, 2,
            // Back face
            4, 5, 6,
            4, 6, 7,
            // Left face
            0, 4, 7,
            0, 7, 3,
            // Right face
            1, 6, 5,
            1, 2, 6,
            // Top face
            3, 6, 2,
            3, 7, 6,
            // Bottom face
            0, 1, 5,
            0, 5, 4
        };

        for (const auto& vertex : vertices) {
            Vertex v;
            v.pos = vertex;
            v.color = {1.0f,1.0f,1.0f,1.0f}; // Default color (white)
            v.texCoord = {0.0f, 0.0f};       // Default texture coordinates
            v.normal = {0.0f, 0.0f, 0.0f};   // Default normal (to be calculated later)
            v.tangent = {0.0f, 0.0f, 0.0f};  // Default tangent (to be calculated later)
            mesh.vertices.push_back(v);
        }

        for (const auto& index : indices) {
            mesh.indices.push_back(index);
        }

        return mesh;
    }

    // Creates a pyramid with apex pointing in +Z (local heading direction).
    // Base lies in the XY plane at z=0; apex is at (0,0,height).
    HostMesh createPyramidMesh(float baseWidth, float baseDepth, float height, glm::vec4 color) {
        HostMesh mesh;

        float hw = baseWidth  * 0.5f;
        float hh = baseDepth  * 0.5f;  // half-extent along Y

        auto insertVertex = [&](glm::vec3 pos, glm::vec3 normal) {
            Vertex v;
            v.pos = pos;
            v.color = color;
            v.texCoord = glm::vec2(0.f);
            v.normal = normal;
            v.tangent = glm::vec3(0.f);
            mesh.vertices.push_back(v);
        };

        // Base vertices in XY plane at z=0, normal faces -Z (back of arrow)
        insertVertex({-hw, -hh, 0}, {0, 0, -1}); // 0
        insertVertex({ hw, -hh, 0}, {0, 0, -1}); // 1
        insertVertex({ hw,  hh, 0}, {0, 0, -1}); // 2
        insertVertex({-hw,  hh, 0}, {0, 0, -1}); // 3

        // Base triangles
        mesh.indices.insert(mesh.indices.end(), {0,2,1, 0,3,2});

        // Side faces – winding {a,b,apex} with n = normalize(cross(b-a, apex-a))
        auto addSide = [&](glm::vec3 a, glm::vec3 b, glm::vec3 apex) {
            glm::vec3 n = glm::normalize(glm::cross(b - a, apex - a));

            uint32_t i0 = mesh.vertices.size();
            insertVertex(a,    n);
            insertVertex(b,    n);
            insertVertex(apex, n);

            mesh.indices.insert(mesh.indices.end(), {i0, i0+1, i0+2});
        };

        glm::vec3 v0{-hw, -hh, 0};
        glm::vec3 v1{ hw, -hh, 0};
        glm::vec3 v2{ hw,  hh, 0};
        glm::vec3 v3{-hw,  hh, 0};
        glm::vec3 apex{0, 0, height};

        addSide(v0, v1, apex); // bottom face
        addSide(v1, v2, apex); // right face
        addSide(v2, v3, apex); // top face
        addSide(v3, v0, apex); // left face

        return mesh;
    }

}