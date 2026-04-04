#include "ClothModel.h"
#include "core/SinglePassRenderer.h"

ClothModel::ClothModel(std::shared_ptr<VulkanContext> ctx, int rows, int cols, float spacing,
                       ClothPinMode pinMode, glm::mat4 initialTransform)
    : Model(ctx, "Cloth", nullptr)
{
    _sim = std::make_unique<ClothSimulation>(_ctx, rows, cols, spacing, pinMode, initialTransform);
}

void ClothModel::setTexture(const std::string& path)
{
    vkDeviceWaitIdle(_ctx->device);
    _texture = std::make_unique<Texture2D>(_ctx, path, VK_FORMAT_R8G8B8A8_SRGB);

    _descriptorSet = std::make_unique<DescriptorSet>(_ctx, std::vector<Descriptor>{
        Descriptor(0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            1,
            _texture->getDescriptorInfo())
    });
}

void ClothModel::draw(VkCommandBuffer cmd, const Renderer& renderer)
{
    auto pipeline = _pipeline.lock();
    if (!pipeline) return;

    pipeline->bind(cmd);

    VkBuffer vb = _sim->getOutParticleBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, _sim->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const auto& spr = static_cast<const SinglePassRenderer&>(renderer);
    VkDescriptorSet sceneds = spr.getSceneDescriptorSet()->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->getPipelineLayout(), 0, 1, &sceneds, 0, nullptr);

    if (_descriptorSet)
    {
        VkDescriptorSet texds = _descriptorSet->getDescriptorSet();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline->getPipelineLayout(), 1, 1, &texds, 0, nullptr);
    }

    vkCmdPushConstants(cmd, pipeline->getPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &_modelMatrix);

    vkCmdDrawIndexed(cmd, _sim->getIndexCount(), 1, 0, 0, 0);
}
