#pragma once

#include "pxe/scene.hpp"

#include <string>

namespace game {

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
};

pxe::Scene buildScene(const BuildOptions& options = {});

}  // namespace game
