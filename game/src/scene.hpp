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
};

pxe::Scene buildScene(const BuildOptions& options = {});

}  // namespace game
