#pragma once

#include "pxe/mesh.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace pxe {

// One grass sprite. The whole quad is lit from `root` and `normal`; the
// fragment shader only tests the atlas alpha.
struct GrassInstance {
  float px, py, pz;
  float nx, ny, nz;
  float sprite;  // 0..2 tuft, 3 claw
  float flip;    // 0 or 1
  float accent;  // <0 regular, else starting accent index in [0, 5)
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

  // R8 atlas, bottom row first (GL). Four sprites in 16px cells.
  std::vector<std::uint8_t> atlas;
  int atlasW = 64;
  int atlasH = 16;
  float spriteRects[4][4] = {};

  glm::vec3 sunDir{0.f, 1.f, 0.f};  // toward the light
  glm::vec3 boundsMin{0.f};
  glm::vec3 boundsMax{0.f};
  glm::vec3 focus{0.f};
};

}  // namespace pxe
