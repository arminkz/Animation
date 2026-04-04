#include "JellyModel.h"
#include "core/SinglePassRenderer.h"

JellyModel::JellyModel(std::shared_ptr<VulkanContext> ctx, JellySimulation* sim)
    : Model(ctx, "Jelly", nullptr), _sim(sim)
{
}

void JellyModel::draw(VkCommandBuffer cmd, const Renderer& renderer)
{
    auto pipeline = _pipeline.lock();
    if (!pipeline) return;

    pipeline->bind(cmd);

    VkBuffer     vb     = _sim->getOutParticleBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, _sim->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const auto& spr    = static_cast<const SinglePassRenderer&>(renderer);
    VkDescriptorSet ds = spr.getSceneDescriptorSet()->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->getPipelineLayout(), 0, 1, &ds, 0, nullptr);

    glm::mat4 identity = glm::mat4(1.f);
    vkCmdPushConstants(cmd, pipeline->getPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &identity);

    vkCmdDrawIndexed(cmd, _sim->getIndexCount(), 1, 0, 0, 0);
}
