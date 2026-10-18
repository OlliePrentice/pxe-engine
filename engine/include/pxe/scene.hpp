#pragma once

#include "pxe/mesh.hpp"

#include <glm/vec3.hpp>

#include <vector>

namespace pxe {

// One grass or leaf sprite, drawn procedurally in the fragment shader. The
// whole quad is lit from `root` and `normal`.
struct GrassInstance {
  float px, py, pz;
  float nx, ny, nz;
  float kind;     // 0 blade clump, 1 tuft, 2 leaf cluster, 3 leaf cluster in the detail colour
  float seed;     // [0, 1), picks blade heights and leans
  float scale;    // blade height multiplier
  float surface;  // Surface whose ramp colours the sprite
};

static_assert(sizeof(GrassInstance) == sizeof(float) * 10, "GrassInstance is tightly packed");

// Colours for one surface, indexed by toon band: 0 shadow, 1 mid, 2 lit.
struct Palette {
  glm::vec3 band[3]{};
  glm::vec3 detail[3]{};  // tufts, leaf speckles, water streaks
  glm::vec3 tip[3]{};     // a blade's top pixel over darker ground
};

struct Scene {
  std::vector<Mesh> meshes;  // opaque; all cast shadows
  std::vector<GrassInstance> grass;
  Palette palette[kSurfCount];

  // N·L thresholds for the toon ramp.
  float tLit = 0.75f;
  float tMid = 0.585f;

  glm::vec3 sunDir{0.f, 1.f, 0.f};  // toward the light
  glm::vec3 boundsMin{0.f};
  glm::vec3 boundsMax{0.f};

  // Where the camera starts.
  glm::vec3 focus{0.f};
  float yawDegrees = 45.f;
};

}  // namespace pxe
