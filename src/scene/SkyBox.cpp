#include "SkyBox.h"

#include "core/SinglePassRenderer.h"
#include "core/AssetPath.h"
#include "geometry/MeshFactory.h"


SkyBox::SkyBox(std::shared_ptr<VulkanContext> ctx, const std::string& textures_path)
    : Model(ctx, "Skybox", std::make_shared<DeviceMesh>(ctx, MeshFactory::createCubeMesh(1.f,1.f,1.f)))
{
    // Setup cubemap texture
    _cubemapTexture = std::make_shared<TextureCubemap>(_ctx,
        AssetPath::getInstance()->get(textures_path), VK_FORMAT_R8G8B8A8_SRGB);

    // Descriptors
    std::vector<Descriptor> descriptors = {
        Descriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 
            1, _cubemapTexture->getDescriptorInfo()), // Cubemap texture
    };
    _descriptorSet = std::make_unique<DescriptorSet>(_ctx, descriptors);

    // Scale the skybox to a large size
    _modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1500.0f));
}


SkyBox::~SkyBox()
{
    // Cleanup resources if needed
}


void SkyBox::draw(VkCommandBuffer commandBuffer, const Renderer& renderer)
{
    const SinglePassRenderer* ssScene = dynamic_cast<const SinglePassRenderer*>(&renderer);

    auto pipeline = _pipeline.lock();
    if (!pipeline) {
        spdlog::error("Pipeline is not set for SkyBox model.");
        return;
    }

    // Bind the pipeline and descriptor set
    pipeline->bind(commandBuffer);
    
    VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, _mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    std::array<VkDescriptorSet, 2> descriptorSets = {
        ssScene->getSceneDescriptorSet()->getDescriptorSet(),        // Scene descriptor set
        _descriptorSet->getDescriptorSet()                           // My descriptor set
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 2, descriptorSets.data(), 0, nullptr);

    // Push constants for model
    vkCmdPushConstants(commandBuffer, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &_modelMatrix);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(_mesh->getIndicesCount()), 1, 0, 0, 0);
}