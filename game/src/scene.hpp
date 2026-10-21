#pragma once

#include "pxe/scene.hpp"

#include <glm/vec2.hpp>

#include <string>

namespace game {

// The ruins' campfire, on open lawn left of the pond (world xz).
constexpr glm::vec2 kRuinsCampfire{-5.f, -6.f};
// Where the fox sleeps in the ruins (world xz): on open lawn 1.8 m from the
// campfire, on its pond side (+x) and a little behind it in the default view
// (yaw 180, looking toward -z). Curled, it faces the fire. Standing, it has
// turned a quarter-turn left (in its clips), so it faces about 19 degrees
// right of straight at that view's camera: its face shows as it looks round.
// Every vertex of every frame of all four of its clips, at 2.4x and tilted
// to the slope, stays at least 0.42 m clear of the fire's stones, logs and
// dirt patch and of everything in ruins.glb (nearest: the paving to its
// right, then the dirt patch, 0.47 m). The ground there is planar: standing,
// all four paws are the same 9 mm over it as on flat ground, so the curl's
// tilt suits the standing fox too.
constexpr glm::vec2 kRuinsFoxSpot{-3.3f, -6.6f};

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
