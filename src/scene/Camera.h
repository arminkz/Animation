#pragma once
#include "stdafx.h"

class Camera
{
public:
    virtual ~Camera() = default;

    virtual void changeZoom(float delta) = 0;
    virtual void rotateHorizontally(float delta) = 0;
    virtual void rotateVertically(float delta) = 0;

    virtual glm::mat4 getViewMatrix() = 0;
    virtual glm::vec3 getPosition()   = 0;
    virtual float     getRadius()  const = 0;
    virtual glm::vec3 getTarget()  const = 0;
    virtual glm::vec3 getForward() const = 0;
    virtual glm::vec3 getLeft()    const = 0;
    virtual glm::vec3 getUp()      const = 0;

    // Animated target — default no-op for cameras that don't support it
    virtual void setTargetAnimated(const glm::vec3&) {}
    virtual void advanceAnimation(float /*deltaTime*/) {}
};
