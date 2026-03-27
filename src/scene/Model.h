#pragma once
#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/Pipeline.h"
#include "core/Renderer.h"
#include "geometry/DeviceMesh.h"

class Renderer;

class Model : public std::enable_shared_from_this<Model>
{
public:
    const std::string& getName() const { return _name; }
    glm::mat4 getModelMatrix() const { return _modelMatrix; }
    glm::vec3 getPosition() const { return glm::vec3(_modelMatrix[3]); }
    const DeviceMesh* getDeviceMesh() const { return _mesh.get(); }

    virtual void draw(VkCommandBuffer commandBuffer, const Renderer& renderer) = 0;
    void setPipeline(std::shared_ptr<Pipeline> pipeline) { _pipeline = std::move(pipeline); }

    // Scene graph
    void addChild(std::shared_ptr<Model> child);
    void propagate(float t);
    virtual void computeLocalMatrix(float t) {}

protected:
    Model(std::shared_ptr<VulkanContext> ctx, std::string name, std::shared_ptr<DeviceMesh> mesh);
    virtual ~Model() {};

    std::shared_ptr<VulkanContext> _ctx;
    std::string _name;
    std::shared_ptr<DeviceMesh> _mesh;
    std::weak_ptr<Pipeline> _pipeline;

    glm::mat4 _modelMatrix;
    glm::mat4 _localMatrix = glm::mat4(1.f);
    std::shared_ptr<Model> _parent;
    std::vector<std::shared_ptr<Model>> _children;
};