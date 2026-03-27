#pragma once
#include "stdafx.h"
#include "Camera.h"


struct TrackballCameraParams
{
    float radius = 16.0f;
    float minRadius = 0.1f;
    float maxRadius = 1000.0f;

    glm::vec3 target = glm::vec3(0.);

    glm::vec3 initialUp = glm::vec3(0., 1., 0.); // Up vector
    glm::vec3 initialForward = glm::vec3(0., 0., -1.); // Forward vector
};


class TrackballCamera : public Camera
{
public:

    explicit TrackballCamera(const TrackballCameraParams& params = TrackballCameraParams());

    void changeZoom(float delta) override;
    void rotateHorizontally(float delta) override;
    void rotateVertically(float delta) override;

    // Calculated properties
    glm::mat4 getViewMatrix() override;
    glm::vec3 getPosition()   override;

    // Raw properties
    float getRadius() const override { return _radius; }
    void setRadius(float radius) { _radius = glm::clamp(radius, _minRadius, _maxRadius); };

    glm::vec3 getTarget()  const override { return _target; }
    void setTarget(const glm::vec3& target);
    void setTargetAnimated(const glm::vec3& target) override;

    glm::vec3 getForward() const override { return _forward; }
    void setForward(const glm::vec3& forward) { _forward = glm::normalize(forward); };

    glm::vec3 getLeft() const override { return _left; }
    void setLeft(const glm::vec3& left) { _left = glm::normalize(left); };

    glm::vec3 getUp() const override { return _up; }
    void setUp(const glm::vec3& up) { _up = glm::normalize(up); };

    void advanceAnimation(float deltaTime) override;

private:
    float _radius;
    float _minRadius;
    float _maxRadius;

    glm::vec3 _target;
    glm::vec3 _forward;
    glm::vec3 _left;
    glm::vec3 _up;

    glm::mat4 _viewMatrix;

    // Animation state
    bool _isAnimating = false;
    glm::vec3 _animationStartTarget;
    glm::vec3 _animationEndTarget;
    float _animationDuration = 1.0f; // in seconds
    float _animationElapsed = 0.0f;

    float easeInOutCubic(float t);
};
