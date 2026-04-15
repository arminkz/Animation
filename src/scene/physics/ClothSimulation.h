#pragma once

#include "scene/physics/MassSpringSimulation.h"
#include "vulkan/ComputePipeline.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/resources/Buffer.h"

// Render vertex for cloth — position/normal updated each frame by compute,
// uv is static (written once at init).
struct ClothVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(ClothVertex), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        return {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ClothVertex, position) },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ClothVertex, normal)   },
            { 2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(ClothVertex, uv)       },
        };
    }
};


enum class ClothPinMode { LeftColumn, TopEdge, None };


class ClothSimulation : public MassSpringSimulation
{
public:
    ClothSimulation(std::shared_ptr<VulkanContext> ctx,
                    int rows, int cols, float spacing,
                    ClothPinMode pinMode = ClothPinMode::LeftColumn,
                    glm::mat4 initialTransform = glm::mat4(1.f));

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
    std::unique_ptr<Buffer> _vertexBuffer;
    uint32_t _numVertices = 0;
    std::unique_ptr<Buffer> _indexBuffer;
    uint32_t _indexCount = 0;

    std::unique_ptr<DescriptorSet> _preRenderDescriptorSets[2];
    std::unique_ptr<ComputePipeline> _preRenderPipeline;
};
