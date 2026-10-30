#pragma once

#include "pxe/first_person_camera.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace desert {

// The player is an eye. Its arms are drawn attached to the view (arms.hpp); it has no body or tools.
constexpr float kEyeHeight = 1.7f;  // metres above the ground, at real scale
constexpr float kWalkSpeed = 3.5f;  // metres a second: the defaults of Player::walkSpeed and sprintSpeed
constexpr float kSprintSpeed = 6.f;
constexpr float kMaxPitch = 85.f * 3.14159265358979323846f / 180.f;  // up or down
// The player is held inside the middle 160 x 160 m of the ground.
constexpr float kPlayHalf = 80.f;
// How far the eye keeps from the palm's trunk cylinder, so the view's near
// plane never cuts into the bark.
constexpr float kBodyRadius = 0.3f;

// What the player asks for this frame, from whichever device. The game's
// input actions (main.cpp) fill it.
struct PlayerInput {
  // Walking, in the view's flat right (x) and forward (y), length 0..1.
  glm::vec2 move{0.f};
  bool sprint = false;
  // Turning, radians: x to the right, y down.
  glm::vec2 look{0.f};
};

// The eye walking on the dunes: where it is and where it looks. The game
// owns it and drives a pxe::FirstPersonCamera from it each frame.
struct Player {
  glm::vec3 eye{0.f};
  float yaw = 0.f;    // as FirstPersonCamera's: 0 looks along +z, larger turns left
  float pitch = 0.f;  // positive looks down
  // Metres a second walking and sprinting: the settings menu's, kWalkSpeed
  // and kSprintSpeed unless it changes them. sprintSpeed > walkSpeed.
  float walkSpeed = kWalkSpeed;
  float sprintSpeed = kSprintSpeed;
  // How far the eye moved over the ground in the last update, metres:
  // across it only, after the trunk and the play area's edge held it.
  float moved = 0.f;

  // Stands the eye on the ground at xz, its height settled.
  void spawn(glm::vec2 xz, float yawRadians, float pitchRadians);
  // Turns, walks and settles the eye over `dt` seconds (the caller clamps a
  // stalled frame's dt). The eye slides along the palm's trunk and the edge
  // of the play area, and eases to standing height over the dunes.
  void update(const PlayerInput& in, float dt);
  // Puts the camera where the eye is, looking where it looks.
  void aim(pxe::FirstPersonCamera& camera) const;
};

}  // namespace desert
