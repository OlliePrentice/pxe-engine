#pragma once

#include "pxe/scene.hpp"

#include <glm/vec2.hpp>

#include <string>

namespace game {

// The ruins' campfire, on open lawn left of the pond (world xz).
constexpr glm::vec2 kRuinsCampfire{-5.f, -6.f};
// How far a campfire's scorched dirt patch reaches from its middle at most
// (its ragged rim is 0.82-0.96 m out; see addCampfire).
constexpr float kCampfireReach = 0.96f;
// Where the fox sleeps in the ruins (world xz): on open lawn 3.2 m from the
// campfire, on its pond side (+x) and behind it in the default view (yaw
// 180, looking toward -z), between the bank and the paving. Curled, it faces
// the fire. Standing, it has turned a quarter-turn left (in its clips), so it
// faces about 18 degrees right of straight at that view's camera: its face
// shows as it looks round. Every vertex of every frame of all five of its
// clips, at 2.4x and tilted to the slope, stays at least 0.2 m clear of the
// fire's stones, logs and dirt patch and of everything in ruins.glb
// (nearest: the paving to its right, 0.204 m as it lies down, then 0.309 m;
// the fire more than 1 m). The ground there is near planar: standing, its
// paws are 10-16 mm over it (9 mm on flat ground), so the curl's tilt suits
// the standing fox too. There is room ahead and to its left for its short
// walks (fox.hpp); to its right the fire and behind it the bank leave none.
constexpr glm::vec2 kRuinsFoxSpot{-2.f, -7.f};

struct BuildOptions {
  // "ruins" (the Blender pond-and-pillars scene) or "hills".
  std::string scene = "ruins";
  // Horizontal Poisson-disk radius for blade clumps, in metres. A clump is
  // about five pixels (0.3 m) wide, so this keeps band edges continuous.
  float minDist = 0.34f;
  // Spacing of the sparse detail tufts.
  float tuftDist = 2.f;
  bool tufts = true;
  // Leaf clusters per square metre of crown.
  float leafDensity = 6.f;
  // Eight isolated sprites instead of the field, for the pixel-exact check
  // (hills only).
  bool stamps = false;
  // Ruins only: add a campfire under a stone shelter and one under a tree
  // crown, to check that rain puts out only the fires open to it.
  bool shelterTest = false;
  // Ruins only: this many bare flames scattered over the lawn, to test many
  // fires at once.
  int extraFires = 0;
  // Ruins only: candles on flat stone tops and on the lawn, and, with the
  // shelter test, under its roof - to check that they light their
  // surroundings and that rain puts out only those open to it.
  bool candles = false;
  // Ruins only: when the fox is shown, the ground it lies on - xz and the
  // radius its curl covers - so no grass (clumps or tufts) grows up through
  // it. A radius of 0 (no fox) leaves the grass as it always was.
  glm::vec2 foxSpot{0.f};
  float foxRadius = 0.f;
};

pxe::Scene buildScene(const BuildOptions& options = {});

}  // namespace game
