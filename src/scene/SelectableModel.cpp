#include "SelectableModel.h"


// Initialize static member
int SelectableModel::_nextId = 1;


SelectableModel::SelectableModel(std::shared_ptr<VulkanContext> ctx, std::string name, std::shared_ptr<DeviceMesh> mesh)
    : Model(std::move(ctx), std::move(name), std::move(mesh)), _id(_nextId++)
{
    // Initialize model matrix to identity
    _modelMatrix = glm::mat4(1.0f);
}