#include "SphereModel.h"
#include "geometry/MeshFactory.h"
#include "geometry/DeviceMesh.h"
#include "core/SinglePassRenderer.h"

SphereModel::SphereModel(std::shared_ptr<VulkanContext> ctx,
                         glm::vec3 center, float radius,
                         int segments, int rings)
    : Model(ctx, "Sphere",
            std::make_shared<DeviceMesh>(ctx, MeshFactory::createSphereMesh(radius, segments, rings)))
{
    _modelMatrix = glm::translate(glm::mat4(1.f), center);
}

void SphereModel::draw(VkCommandBuffer cmd, const Renderer& renderer)
{
    auto pipeline = _pipeline.lock();
    if (!pipeline) return;

    pipeline->bind(cmd);

    VkBuffer vb = _mesh->getVertexBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, _mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const auto& spr = static_cast<const SinglePassRenderer&>(renderer);
    VkDescriptorSet sceneds = spr.getSceneDescriptorSet()->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->getPipelineLayout(), 0, 1, &sceneds, 0, nullptr);

    vkCmdPushConstants(cmd, pipeline->getPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &_modelMatrix);

    vkCmdDrawIndexed(cmd, _mesh->getIndicesCount(), 1, 0, 0, 0);
}
