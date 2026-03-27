#pragma once

#include "stdafx.h"
#include "scene/Model.h"
#include "geometry/HostMesh.h"
#include "geometry/DeviceMesh.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/resources/Texture2D.h"
#include "scene/content/ClothSimulation.h"

class ClothModel : public Model
{
public:
    ClothModel(std::shared_ptr<VulkanContext> ctx, int rows, int cols, float spacing);

    void advance(float dt);
    void draw(VkCommandBuffer commandBuffer, const Renderer& renderer) override;

    ClothSimulation& getSimulation() { return _simulation; }
    const DescriptorSet* getDescriptorSet() const { return _descriptorSet.get(); }
    
    void setTexture(const std::string& path);
    
private:
    ClothSimulation _simulation;
    HostMesh        _hostMesh;
    DeviceMesh*     _dMesh = nullptr; // non-owning - Model base owns the shared_ptr

    std::unique_ptr<Texture2D>     _texture;
    std::unique_ptr<DescriptorSet> _descriptorSet;

    void buildHostMesh();
    void recomputeNormals();
};
