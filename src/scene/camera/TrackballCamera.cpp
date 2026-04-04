#include "TrackballCamera.h"

TrackballCamera::TrackballCamera(const TrackballCameraParams& params) {
    _radius = params.radius;
    _minRadius = params.minRadius;
    _maxRadius = params.maxRadius;
    _target = params.target;

    _up = glm::normalize(params.initialUp);
    _forward = glm::normalize(params.initialForward);
    _left = glm::normalize(glm::cross(_up, _forward));

    _viewMatrix = glm::lookAt(_target + _radius * -1.f * _forward, _target, _up);
}

void TrackballCamera::rotateHorizontally(float delta) {
    // rotate camera frame around the up vector
    glm::quat qYaw = glm::angleAxis(delta, _up);

    _forward = qYaw * _forward;
    _left = glm::cross(_up, _forward);
    _left = glm::normalize(_left);

    _viewMatrix = glm::lookAt(_target + _radius * -1.f * _forward, _target, _up);
}

void TrackballCamera::rotateVertically(float delta) {
    // rotate camera frame around the left vector
    glm::quat qPitch = glm::angleAxis(delta, _left);

    _forward = qPitch * _forward;
    _up = glm::cross(_forward, _left);
    _up = glm::normalize(_up);

    _viewMatrix = glm::lookAt(_target + _radius * -1.f * _forward, _target, _up);
}

void TrackballCamera::changeZoom(float delta) {
    _radius = glm::clamp(_radius + delta, _minRadius, _maxRadius);

    _viewMatrix = glm::lookAt(_target + _radius * -1.f * _forward, _target, _up);
}

glm::mat4 TrackballCamera::getViewMatrix() {
    return _viewMatrix;
}

glm::vec3 TrackballCamera::getPosition() {
    return _target + -1.f * _radius * _forward;
}

void TrackballCamera::setTarget(const glm::vec3& target) {
    if (_isAnimating) {
        _animationEndTarget = target;
    }
    else {
        _target = target;
        _viewMatrix = glm::lookAt(_target + _radius * -1.f * _forward, _target, _up);
    }
}

void TrackballCamera::setTargetAnimated(const glm::vec3& target) {
    _isAnimating = true;
    _animationStartTarget = _target;
    _animationEndTarget = target;
    _animationDuration = 1.0f;
    _animationElapsed = 0.0f;
}

void TrackballCamera::advanceAnimation(float deltaTime) {
    if (_isAnimating) {
        _animationElapsed += deltaTime;
        float t = glm::clamp(_animationElapsed / _animationDuration, 0.0f, 1.0f);
        float easedT = easeInOutCubic(t); // cubic easing function
        _target = glm::mix(_animationStartTarget, _animationEndTarget, easedT);

        _viewMatrix = glm::lookAt(_target + _radius * -1.f * _forward, _target, _up);

        if (t >= 1.0f) {
            _isAnimating = false;
        }
    }
}

float TrackballCamera::easeInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}
