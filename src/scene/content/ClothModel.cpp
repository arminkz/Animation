#include "ClothModel.h"

#include "core/SinglePassRenderer.h"

ClothModel::ClothModel(std::shared_ptr<VulkanContext> ctx, int rows, int cols, float spacing)
    : Model(ctx, "Cloth", nullptr)
{
    _simulation.init(rows, cols, spacing);
    buildHostMesh();

    auto deviceMesh = std::make_shared<DeviceMesh>(_ctx, _hostMesh, /*dynamic=*/true);
    _dMesh = deviceMesh.get();
    _mesh  = std::move(deviceMesh);
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

void ClothModel::advance(float dt)
{
    _simulation.step(dt);

    const auto& particles = _simulation.getParticles();
    for (size_t i = 0; i < particles.size(); ++i)
        _hostMesh.vertices[i].pos = particles[i].position;

    recomputeNormals();
    _dMesh->update(_hostMesh);
}

void ClothModel::draw(VkCommandBuffer cmd, const Renderer& renderer)
{
    auto pipeline = _pipeline.lock();
    if (!pipeline) return;

    pipeline->bind(cmd);

    VkBuffer     vb     = _mesh->getVertexBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, _mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

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

    vkCmdDrawIndexed(cmd, _mesh->getIndicesCount(), 1, 0, 0, 0);
}

void ClothModel::buildHostMesh()
{
    const auto& particles = _simulation.getParticles();
    int rows = _simulation.getRowCount();
    int cols = _simulation.getColCount();

    _hostMesh.vertices.resize(particles.size());
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            int i      = r * cols + c;
            auto& v    = _hostMesh.vertices[i];
            v.pos      = particles[i].position;
            v.color    = glm::vec4(1.f);
            v.normal   = glm::vec3(0.f, 0.f, 1.f);
            v.texCoord = glm::vec2(
                (float)c / (cols - 1),
                1.0f - (float)r / (rows - 1)
            );
            v.tangent  = glm::vec3(1.f, 0.f, 0.f);
        }
    }

    _hostMesh.indices = _simulation.getIndices();
}

void ClothModel::recomputeNormals()
{
    auto& verts = _hostMesh.vertices;
    const auto& indices = _hostMesh.indices;

    for (auto& v : verts)
        v.normal = glm::vec3(0.f);

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        glm::vec3 v0 = verts[indices[i    ]].pos;
        glm::vec3 v1 = verts[indices[i + 1]].pos;
        glm::vec3 v2 = verts[indices[i + 2]].pos;
        glm::vec3 n  = glm::cross(v1 - v0, v2 - v0);
        verts[indices[i    ]].normal += n;
        verts[indices[i + 1]].normal += n;
        verts[indices[i + 2]].normal += n;
    }

    for (auto& v : verts)
    {
        float len = glm::length(v.normal);
        if (len > 1e-6f) v.normal /= len;
    }
}
