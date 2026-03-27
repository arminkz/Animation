#pragma once
#include "stdafx.h"
#include "Camera.h"


struct TurnTableCameraParams
{
    glm::vec3 target = glm::vec3(0.);
    glm::vec3 worldUp = glm::vec3(0., 1., 0.); // World up vector (fixed)

    float initialRadius = 16.0f;
    float minRadius = 0.1f;
    float maxRadius = 1000.0f;

    float initialAzimuth = 0.0f;   // Horizontal angle

    float initialElevation = 0.0f; // Vertical angle
    float minElevation = -glm::half_pi<float>() + 0.01f; // Prevent flipping
    float maxElevation = glm::half_pi<float>() - 0.01f;
};


class TurnTableCamera : public Camera
{
public:

    explicit TurnTableCamera(const TurnTableCameraParams& params = TurnTableCameraParams());

    void changeZoom(float delta) override;
    void rotateHorizontally(float delta) override;
    void rotateVertically(float delta) override;

    // Calculated properties
    glm::mat4 getViewMatrix() override { return _viewMatrix; }
    glm::vec3 getPosition()   override { return _target + -1.f * _radius * _forward; }

    // Raw properties
    glm::vec3 getTarget()  const override { return _target; }
    float     getRadius()  const override { return _radius; }
    glm::vec3 getForward() const override { return _forward; }
    glm::vec3 getLeft()    const override { return _left; }
    glm::vec3 getUp()      const override { return _up; }

    float getAzimuth()   const { return _azimuth; }
    float getElevation() const { return _elevation; }

    void setTarget(const glm::vec3& target) { _target = target; updateCameraVectors(); }
    void setRadius(float radius) { _radius = glm::clamp(radius, _minRadius, _maxRadius); updateCameraVectors(); }
    void setAzimuth(float azimuth) { _azimuth = azimuth; updateCameraVectors(); }
    void setElevation(float elevation) { _elevation = glm::clamp(elevation, _minElevation, _maxElevation); updateCameraVectors(); }

private:

    glm::vec3 _target;
    glm::vec3 _worldUp;

    float _radius;
    float _minRadius;
    float _maxRadius;

    float _azimuth;   // Horizontal angle (yaw)

    float _elevation; // Vertical angle (pitch)
    float _minElevation;
    float _maxElevation;

    // Computed camera frame vectors
    glm::vec3 _forward;
    glm::vec3 _left;
    glm::vec3 _up;

    glm::mat4 _viewMatrix;
    void updateCameraVectors();
};
