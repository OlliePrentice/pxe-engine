#pragma once

#include "pxe/scene.hpp"

namespace game {

struct BuildOptions {
  // Horizontal Poisson-disk radius in metres. ~0.70 lands near 1.8 tufts/m².
  float minDist = 0.62f;
  bool accents = true;
  // Eight isolated sprites instead of the field, for the pixel-exact check.
  bool stamps = false;
};

pxe::Scene buildTerrain(const BuildOptions& options = {});

}  // namespace game
