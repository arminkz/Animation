#include "Model.h"


Model::Model(std::shared_ptr<VulkanContext> ctx, std::string name, std::shared_ptr<DeviceMesh> mesh)
    : _ctx(std::move(ctx)), _name(std::move(name)), _mesh(std::move(mesh))
{
    _modelMatrix = glm::mat4(1.0f);
}

void Model::addChild(std::shared_ptr<Model> child)
{
    child->_parent = shared_from_this();
    _children.push_back(std::move(child));
}

void Model::propagate(float t)
{
    computeLocalMatrix(t);
    if (_parent)
        _modelMatrix = glm::translate(glm::mat4(1.f), _parent->getPosition()) * _localMatrix;
    else
        _modelMatrix = _localMatrix;
    for (auto& child : _children)
        child->propagate(t);
}
