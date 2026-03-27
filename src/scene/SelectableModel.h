#pragma once
#include "stdafx.h"

#include "vulkan/Pipeline.h"
#include "core/Renderer.h"
#include "Model.h"

class SelectableModel : public Model
{
public:
    int getID() const { return _id; }

    virtual void drawSelection(VkCommandBuffer commandBuffer, const Renderer& renderer) = 0;
    void setSelectionPipeline(std::shared_ptr<Pipeline> pipeline) {_selectionPipeline = std::move(pipeline);}
    
protected:
    // Constructor automatically assigns ID
    SelectableModel(std::shared_ptr<VulkanContext> ctx, std::string name, std::shared_ptr<DeviceMesh> mesh);
    virtual ~SelectableModel() {}

    static int _nextId;
    int _id;

    std::weak_ptr<Pipeline> _selectionPipeline;
};