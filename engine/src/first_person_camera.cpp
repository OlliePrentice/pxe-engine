#include "pxe/first_person_camera.hpp"

#include "pxe/config.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace pxe {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMaxPitch = 89.f * kPi / 180.f;
constexpr float kMinFov = 1.f * kPi / 180.f;
constexpr float kMaxFov = 170.f * kPi / 180.f;

}  // namespace

void FirstPersonCamera::setPitchRadians(float pitch) { pitch_ = std::clamp(pitch, -kMaxPitch, kMaxPitch); }

void FirstPersonCamera::setFovYRadians(float fovY) { fovY_ = std::clamp(fovY, kMinFov, kMaxFov); }

void FirstPersonCamera::setFarPlane(float far) { far_ = std::max(far, kNear + 1.f); }

CameraFrame FirstPersonCamera::frame(int rtW, int rtH) const {
  CameraFrame f;
  f.projection = CameraFrame::Projection::kPerspective;
  f.fovY = fovY_;
  f.yawRadians = yaw_;
  f.pitchRadians = pitch_;
  f.pixelsPerUnit = RenderConfig::kPixelsPerUnit;
  f.texelWorld = 1.f / f.pixelsPerUnit;
  f.nearPlane = kNear;
  f.farPlane = far_;
  f.texelError = glm::vec2{0.f};
  f.eye = eye_;

  // As Camera's basis, with a free pitch.
  const float cp = std::cos(pitch_);
  const float sp = std::sin(pitch_);
  const glm::vec3 flatForward{std::sin(yaw_), 0.f, std::cos(yaw_)};
  f.right = glm::vec3{-std::cos(yaw_), 0.f, std::sin(yaw_)};
  f.forward = cp * flatForward + glm::vec3{0.f, -sp, 0.f};
  f.up = glm::cross(f.right, f.forward);
  f.target = eye_ + f.forward;

  // Rows are camera right, camera up and -forward, as Camera builds them.
  f.view[0] = glm::vec4(f.right.x, f.up.x, -f.forward.x, 0.f);
  f.view[1] = glm::vec4(f.right.y, f.up.y, -f.forward.y, 0.f);
  f.view[2] = glm::vec4(f.right.z, f.up.z, -f.forward.z, 0.f);
  f.view[3] = glm::vec4(-glm::dot(f.right, f.eye), -glm::dot(f.up, f.eye), glm::dot(f.forward, f.eye), 1.f);

  const float aspect = static_cast<float>(std::max(rtW, 1)) / static_cast<float>(std::max(rtH, 1));
  f.proj = glm::perspective(fovY_, aspect, kNear, far_);
  return f;
}

}  // namespace pxe
