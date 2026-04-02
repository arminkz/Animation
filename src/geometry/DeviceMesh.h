#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/resources/Buffer.h"
#include "geometry/HostMesh.h"

enum class VertexBufferMode { Static, Dynamic, ComputeWritable };

// Mesh representation on GPU
class DeviceMesh
{
public:
    DeviceMesh(std::shared_ptr<VulkanContext> ctx,
               const HostMesh& mesh,
               VertexBufferMode mode = VertexBufferMode::Static);

    ~DeviceMesh() = default;

    VkBuffer getVertexBuffer() const { return _vertexBuffer->getBuffer(); }
    VkBuffer getIndexBuffer() const { return _indexBuffer->getBuffer(); }
    uint32_t getIndicesCount() const { return _indexCount; }

    void update(const HostMesh& mesh); // for dynamic meshes

private:
    std::shared_ptr<VulkanContext> _ctx;

    std::unique_ptr<Buffer> _vertexBuffer;
    std::unique_ptr<Buffer> _indexBuffer;
    uint32_t _indexCount;
    VertexBufferMode _mode;

    void createVertexBuffer(const HostMesh& mesh);
    void createIndexBuffer(const HostMesh& mesh);
};
