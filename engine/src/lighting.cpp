#include "pxe/lighting.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace pxe {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg = kPi / 180.f;

// Sun elevation, in degrees, at which each phase is fully in effect. Between
// two keys the palettes cross-fade linearly.
constexpr float kNightAt = -14.f;
constexpr float kTwilightAt = -4.f;
constexpr float kGoldenAt = 6.f;
constexpr float kDayAt = 24.f;

// The moon crosses the same arc as the sun, but only between moonrise and
// moonset, a dark hour after sunset and before sunrise. Whichever of the two
// is higher lights the scene, so they swap in those dark hours, while both
// are below the horizon, and the handover never shows.
constexpr float kMoonrise = 19.f;
constexpr float kMoonset = 5.f;

}  // namespace

glm::vec3 moonDirection(const Scene& scene, float hour) {
  // Map clock time onto a sun-equivalent hour: moonrise -> 06:00 and
  // moonset -> 18:00 while the moon is up, the rest of the day stretched
  // over its time below the horizon, continuous all the way round.
  const float up = std::fmod(kMoonset - kMoonrise + 24.f, 24.f);
  const float since = std::fmod(hour - kMoonrise + 48.f, 24.f);
  const float equivalent = (since < up) ? 6.f + since * 12.f / up : 18.f + (since - up) * 12.f / (24.f - up);
  return sunDirection(scene, equivalent);
}

glm::vec3 sunDirection(const Scene& scene, float hour) {
  const float h = (hour - 12.f) / 12.f * kPi;
  const float tilt = (90.f - scene.sunPeakDegrees) * kDeg;
  const glm::vec3 east = glm::normalize(scene.sunEast);
  const glm::vec3 south = glm::normalize(scene.sunSouth);
  const glm::vec3 noon = glm::vec3{0.f, std::cos(tilt), 0.f} + south * std::sin(tilt);
  return glm::normalize(east * -std::sin(h) + noon * std::cos(h));
}

Lighting lightingAt(const Scene& scene, float hour) {
  Lighting out;
  const glm::vec3 sun = sunDirection(scene, hour);
  const float elev = std::asin(std::clamp(sun.y, -1.f, 1.f)) / kDeg;
  out.sunElevationDegrees = elev;

  float w[kPhaseCount] = {};
  auto fade = [](float x, float a, float b) { return std::clamp((x - a) / (b - a), 0.f, 1.f); };
  if (elev >= kDayAt) {
    w[kPhaseDay] = 1.f;
  } else if (elev >= kGoldenAt) {
    const float t = fade(elev, kGoldenAt, kDayAt);
    w[kPhaseDay] = t;
    w[kPhaseGolden] = 1.f - t;
  } else if (elev >= kTwilightAt) {
    const float t = fade(elev, kTwilightAt, kGoldenAt);
    w[kPhaseGolden] = t;
    w[kPhaseTwilight] = 1.f - t;
  } else {
    const float t = fade(elev, kNightAt, kTwilightAt);
    w[kPhaseTwilight] = t;
    w[kPhaseNight] = 1.f - t;
  }

  for (int s = 0; s < kSurfCount; ++s) {
    Palette& p = out.palette[s];
    for (int b = 0; b < 3; ++b) {
      p.band[b] = p.detail[b] = p.tip[b] = glm::vec3{0.f};
      for (int ph = 0; ph < kPhaseCount; ++ph) {
        p.band[b] += w[ph] * scene.palette[ph][s].band[b];
        p.detail[b] += w[ph] * scene.palette[ph][s].detail[b];
        p.tip[b] += w[ph] * scene.palette[ph][s].tip[b];
      }
    }
  }
  out.rim = out.ink = glm::vec3{0.f};
  for (int ph = 0; ph < kPhaseCount; ++ph) {
    out.rim += w[ph] * scene.rim[ph];
    out.ink += w[ph] * scene.ink[ph];
  }

  const glm::vec3 moon = moonDirection(scene, hour);
  out.lightDir = (sun.y >= moon.y) ? sun : moon;
  // Thresholds ease down with the sun, but only so far: just after sunrise
  // the lawn stays dark while upright faces turned to the sun catch the
  // light first, and as the sun climbs the light spreads onto the ground.
  const float flat = std::clamp(out.lightDir.y / std::sin(scene.sunPeakDegrees * kDeg), 0.45f, 1.f);
  out.horizon = glm::vec4{scene.horizonCentre.x, scene.horizonCentre.y, scene.horizonRadius, scene.horizonHeight};
  out.horizonRelief = scene.horizonRelief;
  out.tLit = scene.tLit * flat;
  out.tMid = scene.tMid * flat;
  // Full from golden hour up, gone once night is fully in.
  out.fill = fade(elev, kNightAt, kGoldenAt);
  return out;
}

}  // namespace pxe
