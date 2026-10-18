#include "pxe/camera.hpp"

#include "pxe/config.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace pxe {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kPitch = 30.f * kPi / 180.f;
constexpr float kYawStep = 45.f * kPi / 180.f;
constexpr float kYawDuration = 0.45f;
constexpr float kEyeDistance = 70.f;
constexpr float kNear = 0.5f;
constexpr float kFar = 220.f;

float smoothstep(float t) {
  t = std::clamp(t, 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

// Second-order tracker from t3ssel8r's procedural-animation notes.
struct SecondOrder {
  float k1 = 0.f;
  float k2 = 0.f;
  float k3 = 0.f;

  static SecondOrder from(float f, float zeta, float r) {
    SecondOrder s;
    s.k1 = zeta / (kPi * f);
    s.k2 = 1.f / ((2.f * kPi * f) * (2.f * kPi * f));
    s.k3 = r * zeta / (2.f * kPi * f);
    return s;
  }

  void step(glm::vec3& y, glm::vec3& yd, glm::vec3 x, glm::vec3 xd, float dt) const {
    const float k2s = std::max(k2, std::max(dt * dt * 0.5f + dt * k1 * 0.5f, dt * k1));
    y += dt * yd;
    yd += dt * (x + k3 * xd - y - k1 * yd) / k2s;
  }
};

const SecondOrder kFollow = SecondOrder::from(2.6f, 1.f, 0.f);

}  // namespace

Camera::Camera() {
  glm::vec3 forward, right, up;
  basis(yaw_, forward, right, up);
  // 40 low-res pixels below the view centre, matching the brief's mock so
  // the hill and the lit stripes sit where the reference metrics expect.
  const glm::vec3 focus = up * (40.f / static_cast<float>(RenderConfig::kPixelsPerUnit));
  rawPos_ = smoothPos_ = focus;
}

void Camera::setTarget(glm::vec3 target) {
  rawPos_ = target;
  smoothPos_ = target;
  rawVel_ = glm::vec3{0.f};
  smoothVel_ = glm::vec3{0.f};
}

void Camera::nudgeImmediate(glm::vec3 delta) {
  rawPos_ += delta;
  smoothPos_ += delta;
}

void Camera::setYawRadians(float yaw) {
  yaw_ = yaw;
  yawFrom_ = yaw;
  yawTo_ = yaw;
  yawT_ = 1.f;
  yawAnimating_ = false;
}

void Camera::setPanVelocity(glm::vec2 groundMetresPerSec) { wishPan_ = groundMetresPerSec; }

void Camera::rotateSteps(int steps) {
  if (steps == 0) return;
  yawTo_ += static_cast<float>(steps) * kYawStep;
  yawFrom_ = yaw_;
  yawT_ = 0.f;
  yawAnimating_ = true;
}

void Camera::update(float dt) {
  dt = std::clamp(dt, 0.f, 0.05f);

  glm::vec3 forward, right, up;
  basis(yaw_, forward, right, up);
  glm::vec3 flatForward{forward.x, 0.f, forward.z};
  const float fl = glm::length(flatForward);
  if (fl > 1e-5f) flatForward /= fl;

  rawVel_ = right * wishPan_.x + flatForward * wishPan_.y;
  rawPos_ += rawVel_ * dt;
  kFollow.step(smoothPos_, smoothVel_, rawPos_, rawVel_, dt);

  if (yawAnimating_) {
    yawT_ += dt / kYawDuration;
    if (yawT_ >= 1.f) {
      yawT_ = 1.f;
      yaw_ = yawTo_;
      yawAnimating_ = false;
    } else {
      yaw_ = yawFrom_ + (yawTo_ - yawFrom_) * smoothstep(yawT_);
    }
  }
}

void Camera::basis(float yaw, glm::vec3& forward, glm::vec3& right, glm::vec3& up) const {
  const float cp = std::cos(kPitch);
  const float sp = std::sin(kPitch);
  const glm::vec3 flatForward{std::sin(yaw), 0.f, std::cos(yaw)};
  // Right-handed: looking along +z with y up, right is -x. The old
  // (cos, 0, -sin) here rendered a mirror image of the world.
  right = glm::vec3{-std::cos(yaw), 0.f, std::sin(yaw)};
  forward = cp * flatForward + glm::vec3{0.f, -sp, 0.f};
  up = glm::cross(right, forward);
  if (up.y < 0.f) up = -up;
}

CameraFrame Camera::frame(int rtW, int rtH) const {
  CameraFrame f;
  f.yawRadians = yaw_;
  f.pitchRadians = kPitch;
  f.pixelsPerUnit = RenderConfig::kPixelsPerUnit;
  f.nearPlane = kNear;
  f.farPlane = kFar;
  f.target = smoothPos_;

  glm::vec3 forward, right, up;
  basis(yaw_, forward, right, up);
  f.forward = forward;
  f.right = right;
  f.up = up;

  f.texelWorld = 1.f / static_cast<float>(f.pixelsPerUnit);
  f.orthoHalfW = f.texelWorld * static_cast<float>(rtW) * 0.5f;
  f.orthoHalfH = f.texelWorld * static_cast<float>(rtH) * 0.5f;

  const glm::vec3 desired = smoothPos_ - forward * kEyeDistance;
  const float px = glm::dot(desired, right);
  const float py = glm::dot(desired, up);
  const float pz = glm::dot(desired, forward);
  const float sx = std::round(px / f.texelWorld) * f.texelWorld;
  const float sy = std::round(py / f.texelWorld) * f.texelWorld;
  f.eye = right * sx + up * sy + forward * pz;
  f.texelError = glm::vec2{(px - sx) / f.texelWorld, (py - sy) / f.texelWorld};

  // Rows are camera right, camera up, and -forward, so screen +x is `right`
  // and screen +y is `up`. Built by hand because lookAt() would mirror x
  // for this (right, up, forward) triple.
  f.view[0] = glm::vec4(right.x, up.x, -forward.x, 0.f);
  f.view[1] = glm::vec4(right.y, up.y, -forward.y, 0.f);
  f.view[2] = glm::vec4(right.z, up.z, -forward.z, 0.f);
  f.view[3] = glm::vec4(-glm::dot(right, f.eye), -glm::dot(up, f.eye), glm::dot(forward, f.eye), 1.f);

  f.proj = glm::ortho(-f.orthoHalfW, f.orthoHalfW, -f.orthoHalfH, f.orthoHalfH, kNear, kFar);
  return f;
}

}  // namespace pxe
