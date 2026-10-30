#include "player.hpp"

#include "world.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace desert {
namespace {

// How fast the eye settles to standing height as the ground rises and
// falls under it, per second: light smoothing, so the dunes don't jolt the
// view and it never lags far behind a slope.
constexpr float kEyeEase = 10.f;

// Moves xz out of the palm's trunk cylinder (widened by the body's radius)
// along the line from its axis, keeping the motion round it: the eye slides
// along the trunk.
glm::vec2 outsidePalm(glm::vec2 xz) {
  constexpr float kClear = kPalmRadius + kBodyRadius;
  const glm::vec2 out = xz - kPalmSpot;
  const float distance = glm::length(out);
  if (distance >= kClear) return xz;
  // Exactly on the axis (never reached by walking): out toward -z.
  const glm::vec2 dir = distance > 1e-5f ? out / distance : glm::vec2{0.f, -1.f};
  return kPalmSpot + dir * kClear;
}

}  // namespace

void Player::spawn(glm::vec2 xz, float yawRadians, float pitchRadians) {
  eye = {xz.x, groundHeight(xz.x, xz.y) + kEyeHeight, xz.y};
  yaw = yawRadians;
  pitch = std::clamp(pitchRadians, -kMaxPitch, kMaxPitch);
}

void Player::update(const PlayerInput& in, float dt) {
  // Turning right is a smaller yaw; looking down a larger pitch.
  yaw -= in.look.x;
  pitch = std::clamp(pitch + in.look.y, -kMaxPitch, kMaxPitch);

  const glm::vec2 forward{std::sin(yaw), std::cos(yaw)};  // in x and z
  const glm::vec2 right{-std::cos(yaw), std::sin(yaw)};
  glm::vec2 move = right * in.move.x + forward * in.move.y;
  const float amount = glm::length(move);
  if (amount > 1.f) move /= amount;
  glm::vec2 xz{eye.x, eye.z};
  const glm::vec2 before = xz;
  if (amount > 0.f) {
    xz += move * (in.sprint ? sprintSpeed : walkSpeed) * dt;
    // Each axis held separately at the edge, so walking into it slides
    // along it.
    xz = glm::clamp(xz, glm::vec2{-kPlayHalf}, glm::vec2{kPlayHalf});
    xz = outsidePalm(xz);
  }
  moved = glm::length(xz - before);
  eye.x = xz.x;
  eye.z = xz.y;
  const float standing = groundHeight(xz.x, xz.y) + kEyeHeight;
  eye.y += (standing - eye.y) * (1.f - std::exp(-kEyeEase * dt));
}

void Player::aim(pxe::FirstPersonCamera& camera) const {
  camera.setEye(eye);
  camera.setYawRadians(yaw);
  camera.setPitchRadians(pitch);
}

}  // namespace desert
