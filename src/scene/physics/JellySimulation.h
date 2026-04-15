#pragma once

#include "scene/physics/MassSpringSimulation.h"
#include "vulkan/resources/Buffer.h"

// Vertex structure used for rendering the jelly.
// each vertex is mapped to a particle in physics simulation
struct JellyVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    uint32_t physicsIdx;

    static VkVertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(JellyVertex), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        return {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(JellyVertex, position) },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(JellyVertex, normal)   },
            { 2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(JellyVertex, uv)       },
        };
    }
};

class JellySimulation : public MassSpringSimulation
{
public:
    JellySimulation(std::shared_ptr<VulkanContext> ctx,
                    int resX, int resY, int resZ,
                    float spacing,
                    glm::vec3 center = glm::vec3(0.f));

    VkBuffer getVertexBuffer() const { return _vertexBuffer->getBuffer(); }
    VkBuffer getIndexBuffer()  const { return _indexBuffer->getBuffer(); }
    uint32_t getIndexCount()   const { return _indexCount; }

    // Dispatch the pre-render compute pass. Reads from the
    // currently-latest physics particle buffer (selected via `_currentIn`).
    // Calculates normals and vertex positions from partciles.
    // Must be called *after* the last physics substep and a compute-to-compute
    // barrier, so that the physics writes are visible.
    void dispatchPreRender(VkCommandBuffer cmd);

private:

    // Buffers used for rendering
    std::unique_ptr<Buffer> _vertexBuffer;
    std::unique_ptr<Buffer> _indexBuffer;
    uint32_t _indexCount = 0;

    // Buffers used for pre-render pass
    std::unique_ptr<Buffer> _renderVertexTriRangeBuffer;
    std::unique_ptr<Buffer> _renderTrianglesBuffer;    
    uint32_t _numRenderVertices = 0;

    void initPreRenderBuffers(const std::vector<JellyVertex>& renderVertices,
                              const std::vector<uint32_t>& renderIndices,
                              const std::vector<MSTriangle>& renderTriangles,
                              const std::vector<MSParticleTriRange>& renderVertexTriRanges);

    // One descriptor set per physics ping-pong state so we always read the
    // freshly-written physics buffer.
    std::unique_ptr<DescriptorSet> _preRenderDescriptorSets[2];
    std::unique_ptr<ComputePipeline> _preRenderPipeline;
};
