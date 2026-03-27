#pragma once

#include "stdafx.h"

struct ClothParticle
{
    glm::vec3 position;
    glm::vec3 prevPosition;
    glm::vec3 force;
    float     mass   = 1.0f;
    bool      pinned = false;
};

enum class SpringType { Structural, Shear, Bend };

struct ClothSpring
{
    int        a, b;
    float      restLength;
    float      ratio; // relative stiffness weight (1.0=structural, 0.6=shear, 0.4=bend)
    SpringType type;
};

class ClothSimulation
{
public:
    void init(int rows, int cols, float spacing);
    void step(float dt);

    const std::vector<ClothParticle>& getParticles() const { return _particles; }
    const std::vector<uint32_t>&      getIndices()   const { return _indices; }
    int getRowCount() const { return _rows; }
    int getColCount() const { return _cols; }

    // Mass-Spring
    float stiffness      = 1000.f; // global spring stiffness (N/m)
    float damping        = 0.98f;  // velocity decay per step [0,1]
    float springDamping  = 2.f;    // damping force along spring axis — kills oscillation
    int   substeps       = 8;      // substeps per frame — improves stability with large dt
    // Structure
    bool  shearEnabled   = true;
    bool  bendEnabled    = true;
    // Gravity
    bool  gravityEnabled = true;
    // Wind
    bool      windEnabled    = true;
    float     windStrength   = 50.f;
    float     windTurbulence = 5.f;
    float     windDragCoeff  = 5.f; // Cd — scales aero force per unit area
    glm::vec3 windDirection  = glm::vec3(0.2f, 0.3f, 1.f);

private:
    std::vector<ClothParticle> _particles;
    std::vector<ClothSpring>   _springs;
    std::vector<uint32_t>      _indices;
    int   _rows = 0, _cols = 0;
    float _time = 0.f;

    void applyForces(float dt);
    void integrate(float dt);

    int idx(int r, int c) const { return r * _cols + c; } // Converts 2D index to 1D
};
