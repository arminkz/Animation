#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/ComputePipeline.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/resources/Buffer.h"

// GPU-side particle: ping-pong buffer, also used directly as vertex buffer.
struct MSParticle {
    glm::vec3 position;
    glm::vec3 prevPosition;
    glm::vec3 normal;
    glm::vec2 uv;
    float invMass;

    static VkVertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(MSParticle), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        return {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MSParticle, position) },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MSParticle, normal)   },
            { 2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(MSParticle, uv)       },
        };
    }
};

// Per-particle index into the directed spring list
struct MSParticleMetadata {
    uint32_t springStartIndex;
    uint32_t springCount;
};

// Directed spring edge — target particle only (source = current thread id)
struct MSSpring {
    int32_t neighborIndex;
    float restLength, ratio;
};

// Per-vertex triangle adjacency entry
struct MSVertexTriMetadata {
    uint32_t triStartIndex;
    uint32_t triCount;
};

// Triangle (indices into particle buffer)
struct MSTriangle {
    uint32_t a, b, c;
};

// Collider types
enum class MSColliderType : int32_t { Sphere = 0, Plane = 1 };

// GPU collider — must match Collider struct in mass_spring_integrate.comp exactly
struct MSCollider {
    int32_t type;       // MSColliderType
    glm::vec3 position; // sphere: center,  plane: point on plane
    glm::vec3 normal;   // plane: outward normal (unused for sphere)
    float radius;       // sphere: radius (unused for plane)
    float stiffness;    // penalty spring stiffness (controls bounce strength)
    float friction;     // Coulomb friction coefficient (0 = frictionless, 1 = high grip)
};

// Uniform buffer — must match SimParamsUBO in mass_spring_integrate.comp exactly
struct MSSimParams {
    int32_t numParticles;                                // Size
    float dt, time;                                      // Elapsed / Absolute time
    float stiffness, velocityDamping, springDamping;     // Mass-spring parameters
    glm::vec3 windDir;                                   // Wind
    float windEnabled, windTurbulence, windStrength, windDragCoeff;
    int32_t gravityEnabled;                              // Gravity
    int32_t numColliders;                                // Collision
};


class MassSpringSimulation
{
public:
    virtual ~MassSpringSimulation() = default;

    // Simulation parameters
    // used to fill MassSpringSimParams UBO each frame by the scene
    float stiffness      = 1000.f;
    float damping        = 0.98f;
    float springDamping  = 7.f;
    int   substeps       = 8;
    bool  gravityEnabled = true;
    bool  windEnabled    = true;
    float windStrength   = 30.f;
    float windTurbulence = 7.f;
    float windDragCoeff  = 4.f;
    glm::vec3 windDirection = glm::vec3(0.05f, 0.2f, 1.f);

    // Record one substep into the command buffer
    void dispatch(VkCommandBuffer cmd, float dt, float time);

    // Upload colliders to GPU
    // call before the first dispatch each frame (or whenever they change)
    void setColliders(const std::vector<MSCollider>& colliders);

    // The last-written particle buffer — used as vertex buffer for rendering
    VkBuffer getOutParticleBuffer() const { return _particleBuffers[_currentIn]->getBuffer(); }
    VkBuffer getIndexBuffer() const { return _indexBuffer->getBuffer(); }
    uint32_t getIndexCount() const { return _indexCount; }

protected:
    MassSpringSimulation(std::shared_ptr<VulkanContext> ctx);

    // Called by subclass constructor after building topology
    void init(const std::vector<MSParticle>& particles,
              const std::vector<MSParticleMetadata>& metadata,
              const std::vector<MSSpring>& directedSprings,
              const std::vector<uint32_t>& indices,
              const std::string& shaderSpvPath);

    std::shared_ptr<VulkanContext> _ctx;
    uint32_t _numParticles = 0;
    uint32_t _indexCount = 0;

private:
    std::unique_ptr<Buffer> _paramsBuffer;
    std::unique_ptr<Buffer> _particleBuffers[2];      // ping-pong
    std::unique_ptr<Buffer> _particleMetadataBuffer;
    std::unique_ptr<Buffer> _springsBuffer;
    std::unique_ptr<Buffer> _vertexTriMetaBuffer;
    std::unique_ptr<Buffer> _trianglesBuffer;
    std::unique_ptr<Buffer> _indexBuffer;

    std::unique_ptr<Buffer> _colliderBuffer;
    static constexpr int MAX_COLLIDERS = 16;
    int32_t _numColliders = 0;

    std::unique_ptr<DescriptorSet> _descriptorSets[2];
    std::unique_ptr<ComputePipeline> _computePipeline;

    int _currentIn = 0; //ping or pong     
};
