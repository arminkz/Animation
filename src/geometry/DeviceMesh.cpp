#include "DeviceMesh.h"

#include "vulkan/VulkanHelper.h"

DeviceMesh::DeviceMesh(std::shared_ptr<VulkanContext> ctx, const HostMesh& mesh, bool dynamic)
    : _ctx(std::move(ctx)), _dynamic(dynamic)
{
    _indexCount = static_cast<uint32_t>(mesh.indices.size());
    createVertexBuffer(mesh);
    createIndexBuffer(mesh);
}

void DeviceMesh::update(const HostMesh& mesh)
{
    VkDeviceSize size = sizeof(mesh.vertices[0]) * mesh.vertices.size();
    _vertexBuffer->copyData(mesh.vertices.data(), size);
}

void DeviceMesh::createVertexBuffer(const HostMesh& mesh)
{
    VkDeviceSize bufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size();

    if (_dynamic)
    {
        // Host-visible buffer kept persistently mapped — CPU writes directly each frame
        _vertexBuffer = std::make_unique<Buffer>(_ctx,
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        _vertexBuffer->copyData(mesh.vertices.data(), bufferSize);
    }
    else
    {
        // Staging -> device-local (optimal for static meshes)
        Buffer staging(_ctx,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        staging.copyData(mesh.vertices.data(), bufferSize);

        _vertexBuffer = std::make_unique<Buffer>(_ctx,
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), _vertexBuffer->getBuffer(), bufferSize);
        // staging goes out of scope here — Buffer destructor cleans it up
    }
}

void DeviceMesh::createIndexBuffer(const HostMesh& mesh)
{
    VkDeviceSize bufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();

    Buffer staging(_ctx,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    staging.copyData(mesh.indices.data(), bufferSize);

    _indexBuffer = std::make_unique<Buffer>(_ctx,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VulkanHelper::copyBuffer(_ctx, staging.getBuffer(), _indexBuffer->getBuffer(), bufferSize);
}
