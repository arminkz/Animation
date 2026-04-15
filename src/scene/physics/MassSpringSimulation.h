#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/ComputePipeline.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/resources/Buffer.h"

// GPU-side particle
struct MSParticle {
    glm::vec3 position;
    glm::vec3 prevPosition;
    glm::vec3 normal;
    float invMass;

    // useful if we want to use particles directly as vertices (e.g. in cloth)
    static VkVertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(MSParticle), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        return {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MSParticle, position) },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MSParticle, normal)   },
        };
    }
};

// Per-particle index into the directed spring list
struct MSParticleSprRange {
    uint32_t springStartIndex;
    uint32_t springCount;
};

// Directed spring edge — target particle only (source = current thread id)
struct MSSpring {
    int32_t neighborIndex;
    float restLength, ratio;
};

// Per-particle triangle adjacency entry
struct MSParticleTriRange {
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
    int32_t numParticles;                                           // number of particles
    int32_t numColliders;                                           // number of colliders
    float dt, time;                                                 // Elapsed / Absolute time
    float stiffness, velocityDamping, springDamping;                // Mass-spring parameters
    int32_t gravityEnabled;                                         // Gravity
    int32_t windEnabled;                                            // Wind
    float windTurbulence, windStrength, windDragCoeff;              
    glm::vec3 windDir;                                   
};


class MassSpringSimulation
{
public:
    virtual ~MassSpringSimulation() = default;

    // Simulation parameters
    // used to fill MassSpringSimParams UBO each frame by the scene
    float stiffness        = 1000.f;
    float velocityDamping  = 0.98f;
    float springDamping    = 7.f;
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

    VkBuffer getParticleBuffer(int i) const { return _particleBuffers[i]->getBuffer(); }
    VkBuffer getLastParticleBuffer()  const { return _particleBuffers[_currentIn]->getBuffer(); }

protected:
    MassSpringSimulation(std::shared_ptr<VulkanContext> ctx);

    // Called by subclass constructor after building topology.
    // `triangleIndices` is used only to build the per-vertex triangle adjacency
    // for wind-drag normals. Subclasses that render must upload their own index buffer.
    void init(const std::vector<MSParticle>& particles,
              const std::vector<MSParticleSprRange>& sprRanges,
              const std::vector<MSSpring>& directedSprings,
              const std::vector<uint32_t>& triangleIndices);

    std::shared_ptr<VulkanContext> _ctx;

    int _currentIn = 0;  // ping-pong index
    
private:
    uint32_t _numParticles = 0;
    uint32_t _numColliders = 0;
    static constexpr int MAX_COLLIDERS = 16;

    std::unique_ptr<Buffer> _particleBuffers[2]; // ping-pong
    std::unique_ptr<Buffer> _sprRangesBuffer;
    std::unique_ptr<Buffer> _springsBuffer;
    std::unique_ptr<Buffer> _triRangesBuffer;
    std::unique_ptr<Buffer> _trianglesBuffer;
    std::unique_ptr<Buffer> _colliderBuffer;
    std::unique_ptr<Buffer> _paramsBuffer;
    
    std::unique_ptr<DescriptorSet> _descriptorSets[2];
    std::unique_ptr<ComputePipeline> _computePipeline;
};
