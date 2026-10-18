#pragma once

#include "pxe/scene.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace pxe {

// Everything the renderer needs to light one frame.
struct Lighting {
  glm::vec3 lightDir{0.f, 1.f, 0.f};  // toward the sun by day, the moon by night
  float tLit = 0.75f;
  float tMid = 0.585f;
  Palette palette[kSurfCount];
  glm::vec3 rim{1.f};
  glm::vec3 ink{0.f};
  float sunElevationDegrees = 90.f;
  // Cloud shadows: 0 none, 1 overcast. The offset scrolls the noise in
  // cloud-texture units.
  float cloudCover = 0.f;
  float cloudStrength = 0.6f;  // how far a thick cloud pulls toward shadow
  glm::vec2 cloudOffset{0.f};
  // Horizon ridge, copied from the scene: centre xz, radius, height, relief.
  glm::vec4 horizon{0.f};
  float horizonRelief = 0.f;
  // Horizontal wind direction (unit, world XZ) for the grass sway.
  glm::vec2 windDir{1.f, 0.f};
};

// The scene lit at `hour` (0-24). Palettes blend day -> golden -> twilight
// -> night by sun elevation. The sun or the moon, whichever is higher, is
// the light; the moon crosses the same arc as the sun, through the night.
// Clouds are left at zero for the caller to set.
Lighting lightingAt(const Scene& scene, float hour);

// Where the sun and moon are at `hour`, toward them.
glm::vec3 sunDirection(const Scene& scene, float hour);
glm::vec3 moonDirection(const Scene& scene, float hour);

}  // namespace pxe
