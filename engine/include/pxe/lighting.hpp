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
  // Rain severity, 0 dry .. 1 downpour, and the overcast it brings: how far
  // every colour is pulled toward its shadow tone.
  float rain = 0.f;
  float overcast = 0.f;
  // A lightning flash, 0 none .. 1 at its peak, and the direction toward
  // it. It is a second light with its own hard shadows, blended over the
  // sun or moon by its strength.
  float flash = 0.f;
  glm::vec3 flashDir{0.f, 1.f, 0.f};
  // God rays: how strong the sunbeams are (0 none; the renderer adds the
  // camera angle) and their colour.
  float rays = 0.f;
  glm::vec3 rayColour{1.f};
  // Firefly light: how strong, how far direct sun or moon light drowns it
  // out (0..1), and each surface's colour in full firefly light.
  float fireflyLight = 0.f;
  float fireflyDim = 0.f;
  glm::vec3 fireflyBand[kSurfCount]{};
  // Firelight from the scene's campfires: how strong a fully burning one is
  // (0 none; the renderer scales it by each fire's level and flicker) and
  // each surface's colour in full firelight.
  float fireLight = 0.f;
  float fireReach = 6.f;  // metres
  float fireDim = 0.f;    // how far direct sun or moon light drowns it out
  glm::vec3 fireBand[kSurfCount]{};
  // Horizontal wind direction (unit, world XZ) for the grass sway, and how
  // hard it blows (the drift-speed setting, 1 by default).
  glm::vec2 windDir{1.f, 0.f};
  float windStrength = 1.f;
};

// The weather at one moment, as the game decides it: what lightingAt needs
// on top of the time of day.
struct Weather {
  // Cloud shadows: cover 0 none .. 1 overcast, the noise offset in
  // cloud-texture units, and how far a thick cloud pulls toward shadow.
  float cloudCover = 0.f;
  glm::vec2 cloudOffset{0.f};
  float cloudStrength = 0.6f;
  // Horizontal direction the wind blows toward (unit, world XZ), and how
  // hard it blows (1 by default).
  glm::vec2 windDir{1.f, 0.f};
  float windStrength = 1.f;
  // Rain severity, 0 dry .. 1 downpour.
  float rain = 0.f;
  // A lightning flash, 0 none .. 1 at its peak, and the direction toward it.
  float flash = 0.f;
  glm::vec3 flashDir{0.f, 1.f, 0.f};
};

// The scene lit at `hour` (0-24). Palettes blend day -> golden -> twilight
// -> night by sun elevation. The sun or the moon, whichever is higher, is
// the light; the moon crosses the same arc as the sun, through the night.
// Clouds are left at zero for the caller to set.
Lighting lightingAt(const Scene& scene, float hour);

// The scene lit at `hour` under `weather`: the above, plus the clouds and
// wind, the overcast and fading cloud shadows rain brings, god rays from a
// low sun, firefly and firelight colours, and a lightning flash's wash.
Lighting lightingAt(const Scene& scene, float hour, const Weather& weather);

// Where the sun and moon are at `hour`, toward them.
glm::vec3 sunDirection(const Scene& scene, float hour);
glm::vec3 moonDirection(const Scene& scene, float hour);

}  // namespace pxe
