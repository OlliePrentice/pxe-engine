#include "pxe/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace pxe {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kPitch = 30.f * kPi / 180.f;
constexpr float kYawStep = 45.f * kPi / 180.f;
constexpr float kYawDuration = 0.5f;
constexpr float kEyeDistance = 48.f;

// Integer low-res pixels per world unit. Zoom only rests on these so a
// resting camera keeps a stable texel grid. Index 4 (42) frames a ruin the
// way the reference scenes do: a pillar is about a quarter of the frame.
constexpr int kPixelsPerUnit[] = {12, 16, 22, 30, 42};
constexpr int kZoomCount = 5;

float smoothstep(float t) {
  t = std::clamp(t, 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

// Second-order tracker. k1/k2/k3 come from frequency f, damping zeta and
// initial response r, as described in t3ssel8r's procedural animation video.
// The k2 clamp is the stability limit he discusses for large dt.
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

void Camera::setPixelsPerUnitIndex(int index) {
  zoom_ = std::clamp(index, 0, kZoomCount - 1);
}

void Camera::setPanVelocity(glm::vec2 groundUnitsPerSec) { wishPan_ = groundUnitsPerSec; }

void Camera::rotateSteps(int steps) {
  if (steps == 0) return;
  yawTo_ += static_cast<float>(steps) * kYawStep;
  yawFrom_ = yaw_;
  yawT_ = 0.f;
  yawAnimating_ = true;
}

void Camera::zoomSteps(int steps) {
  zoom_ = std::clamp(zoom_ + steps, 0, kZoomCount - 1);
}

int Camera::pixelsPerUnit() const { return kPixelsPerUnit[zoom_]; }

void Camera::update(float dt) {
  dt = std::clamp(dt, 0.f, 0.05f);

  glm::vec3 forward, right, up;
  recomputeBasis(forward, right, up);
  glm::vec3 flatForward{forward.x, 0.f, forward.z};
  const float fl = glm::length(flatForward);
  if (fl > 1e-5f) flatForward /= fl;
  glm::vec3 flatRight{right.x, 0.f, right.z};
  const float rl = glm::length(flatRight);
  if (rl > 1e-5f) flatRight /= rl;

  rawVel_ = flatRight * wishPan_.x + flatForward * wishPan_.y;
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

void Camera::recomputeBasis(glm::vec3& forward, glm::vec3& right, glm::vec3& up) const {
  const float cp = std::cos(kPitch);
  const float sp = std::sin(kPitch);
  const float cy = std::cos(yaw_);
  const float sy = std::sin(yaw_);
  // Pitch is measured down from the horizon. Yaw 0 looks toward -Z.
  forward = glm::normalize(glm::vec3{sy * cp, -sp, -cy * cp});
  right = glm::normalize(glm::cross(forward, glm::vec3{0.f, 1.f, 0.f}));
  up = glm::cross(right, forward);
}

CameraFrame Camera::frame(int rtW, int rtH, int viewH) const {
  CameraFrame f;
  f.yawRadians = yaw_;
  f.pitchRadians = kPitch;
  f.pixelsPerUnit = pixelsPerUnit();
  f.target = smoothPos_;

  glm::vec3 forward, right, up;
  recomputeBasis(forward, right, up);
  f.forward = forward;
  f.right = right;
  f.up = up;

  // One low-res texel covers this much of the view plane. The projection
  // covers the padded render target, so the border texels are real geometry.
  f.texelWorld = 1.f / static_cast<float>(f.pixelsPerUnit);
  f.orthoHalfW = f.texelWorld * static_cast<float>(rtW) * 0.5f;
  f.orthoHalfH = f.texelWorld * static_cast<float>(rtH) * 0.5f;
  (void)viewH;

  const glm::vec3 desired = smoothPos_ - forward * kEyeDistance;

  // Snap in the camera basis, not in world XYZ. At 45° yaw a world-axis
  // grid is diagonal on screen and the pixels swim.
  const float px = glm::dot(desired, right);
  const float py = glm::dot(desired, up);
  // Snap depth as well. An ortho slide along the view plane still leaks a
  // few ulps into this axis, and that is enough to flip a 1px shore or an
  // outline. Quantizing it keeps the eye bitwise-stable inside one texel.
  const float pz = std::round(glm::dot(desired, forward) / f.texelWorld) * f.texelWorld;
  const float sx = std::round(px / f.texelWorld) * f.texelWorld;
  const float sy = std::round(py / f.texelWorld) * f.texelWorld;
  f.eye = right * sx + up * sy + forward * pz;
  f.texelError = glm::vec2{(px - sx) / f.texelWorld, (py - sy) / f.texelWorld};

  f.view = glm::lookAt(f.eye, f.eye + forward, up);
  f.proj = glm::ortho(-f.orthoHalfW, f.orthoHalfW, -f.orthoHalfH, f.orthoHalfH, 0.5f, 140.f);
  return f;
}

}  // namespace pxe
