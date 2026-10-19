#pragma once

#include "settings.hpp"

#include "pxe/scene.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace game {

// A ball of fire the player rolls about, after the one t3ssel8r rolls
// through the grass to spread his fire. It is a small fire of the game's
// (WorldClock::Fire) whose base follows the ball, so its flames trail behind
// it and it lights the scene through the point lights; it sets the grass
// it rolls over alight. It rolls over the ground, downhill of its own
// accord, steered by a push, and bounces off anything standing more than
// a step high. Rain puts it out like any fire open to the sky, water at
// once; it rolls to a stop, smokes, and is gone.
class Fireball {
 public:
  static constexpr float kSize = 0.45f;  // of a campfire's flames

  bool alive() const { return alive_; }
  glm::vec3 position() const { return {pos_.x, y_, pos_.y}; }

  // Set a new ball rolling from xz along `heading` (world xz), replacing
  // any ball already out.
  void launch(WorldClock& clock, const pxe::Scene& scene, glm::vec2 xz, glm::vec2 heading);
  // Roll on by dt seconds. `push` (world xz, length up to 1) steers it.
  // Adds to `ignite` the points where it sets the grass alight.
  void update(WorldClock& clock, const pxe::Scene& scene, float dt, glm::vec2 push, std::vector<glm::vec2>& ignite);

 private:
  WorldClock::Fire* fire(WorldClock& clock) const;

  bool alive_ = false;
  std::uint32_t fire_ = 0;
  glm::vec2 pos_{0.f};
  glm::vec2 vel_{0.f};
  float y_ = 0.f;
  float sinceIgnite_ = 0.f;  // metres rolled or seconds sat since the last ignition
};

}  // namespace game
