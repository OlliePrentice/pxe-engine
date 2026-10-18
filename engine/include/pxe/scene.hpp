#pragma once

#include "pxe/mesh.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace pxe {

// One grass sprite, drawn procedurally in the fragment shader. The whole quad
// is lit from `root` and `normal`.
struct GrassInstance {
  float px, py, pz;
  float nx, ny, nz;
  float kind;   // 0 clump in the ground colour, 1 tuft in the detail colour
  float seed;   // [0, 1), picks blade heights and leans
  float scale;  // blade height multiplier
  float pad;
};

static_assert(sizeof(GrassInstance) == sizeof(float) * 10, "GrassInstance is tightly packed");

struct Scene {
  Mesh terrain;
  std::vector<GrassInstance> grass;

  // RGBA32F-sized float buffer, stored as RGBA16F on the GPU. xyz is the
  // unit normal. Row 0 is z = worldMin.
  std::vector<float> normals;
  int normalSize = 0;
  float worldMin = -45.f;
  float worldSize = 90.f;

  glm::vec3 sunDir{0.f, 1.f, 0.f};  // toward the light
  glm::vec3 boundsMin{0.f};
  glm::vec3 boundsMax{0.f};
  glm::vec3 focus{0.f};
};

}  // namespace pxe
