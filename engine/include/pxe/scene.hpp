#pragma once

#include "pxe/mesh.hpp"

#include <glm/vec3.hpp>

#include <vector>

namespace pxe {

// What the renderer draws. The game owns construction; the engine does not
// know about ruins, trees, or weather.
struct Scene {
  Mesh opaque;
  Mesh shadowCasters;  // opaque plus canopy proxies that are invisible in colour
  Mesh water;
  std::vector<FoliageInstance> foliage;

  // Horizontal angle of the sun at hour 0. The day/night cycle orbits from here.
  float sunAzimuth = 0.f;
  glm::vec3 focus{0.f};
};

}  // namespace pxe
