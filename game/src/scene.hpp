#pragma once

#include "pxe/scene.hpp"

namespace game {

struct BuildOptions {
  // Horizontal Poisson-disk radius for blade clumps, in metres. A clump is
  // about five pixels (0.3 m) wide, so this keeps band edges continuous.
  float minDist = 0.34f;
  // Spacing of the sparse detail tufts.
  float tuftDist = 2.f;
  bool tufts = true;
  // Eight isolated sprites instead of the field, for the pixel-exact check.
  bool stamps = false;
};

pxe::Scene buildTerrain(const BuildOptions& options = {});

}  // namespace game
