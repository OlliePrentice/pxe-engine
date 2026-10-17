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

  glm::vec3 sunDirection{0.f, 1.f, 0.f};  // direction the light travels
  glm::vec3 shadowCenter{0.f};
  float shadowExtent = 24.f;
  glm::vec3 focus{0.f};
};

}  // namespace pxe
