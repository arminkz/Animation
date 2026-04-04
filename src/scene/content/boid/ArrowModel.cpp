#include "ArrowModel.h"

#include "geometry/MeshFactory.h"
#include "geometry/DeviceMesh.h"
#include "core/SinglePassRenderer.h"

ArrowModel::ArrowModel(std::shared_ptr<VulkanContext> ctx)
    : Model(ctx, "Arrow",
            std::make_shared<DeviceMesh>(ctx,
                MeshFactory::createPyramidMesh(1.f, 1.f, 3.f, {1.f, 0.87f, 0.13f, 1.f})))
{
}

void ArrowModel::setTransformBuffer(VkBuffer buf, uint32_t instanceCount)
{
    _transformBuffer = buf;
    _instanceCount   = instanceCount;

    VkDescriptorBufferInfo info{};
    info.buffer = buf;
    info.offset = 0;
    info.range  = VK_WHOLE_SIZE;

    _transformDS = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
        Descriptor(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                   VK_SHADER_STAGE_VERTEX_BIT, 1, info),
    });
}

void ArrowModel::draw(VkCommandBuffer cmd, const Renderer& renderer)
{
    if (_instanceCount == 0 || _transformBuffer == VK_NULL_HANDLE) return;

    auto pipeline = _pipeline.lock();
    if (!pipeline) return;

    pipeline->bind(cmd);

    VkBuffer     vb     = _mesh->getVertexBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, _mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const auto& spr = static_cast<const SinglePassRenderer&>(renderer);

    // set=0: scene UBO,  set=1: transforms SSBO
    VkDescriptorSet sets[2] = {
        spr.getSceneDescriptorSet()->getDescriptorSet(),
        _transformDS->getDescriptorSet(),
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->getPipelineLayout(), 0, 2, sets, 0, nullptr);

    vkCmdDrawIndexed(cmd, _mesh->getIndicesCount(), _instanceCount, 0, 0, 0);
}
