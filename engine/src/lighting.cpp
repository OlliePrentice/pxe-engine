#include "pxe/lighting.hpp"

#include <glm/common.hpp>
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
  return out;
}

Lighting lightingAt(const Scene& scene, float hour, const Weather& weather) {
  Lighting lighting = lightingAt(scene, hour);
  lighting.cloudCover = weather.cloudCover;
  lighting.cloudOffset = weather.cloudOffset;
  lighting.cloudStrength = weather.cloudStrength;
  lighting.windDir = weather.windDir;
  lighting.windStrength = weather.windStrength;
  // Rain brings a murky, even overcast that fades sun and moon shadows. The
  // drifting cloud shadows fade out as it sets in - under a rain sky there
  // are no separate clouds to see - and are gone from moderate rain up.
  lighting.rain = weather.rain;
  lighting.overcast = 0.7f * weather.rain;
  lighting.cloudStrength *= std::clamp(1.f - 2.f * weather.rain, 0.f, 1.f);

  // The midday colours, which the firefly, firelight and flash colours are
  // drawn from.
  const Lighting noon = lightingAt(scene, 12.f);

  // God rays: only from the sun. Strong in the morning and at dusk while it
  // is low, barely there with it high. A drizzle catches the light and
  // brings them out more; from moderate rain up the sun is gone and so are
  // they. Cloud shadows block them where they fall (in the edge pass).
  {
    const float elev = lighting.sunElevationDegrees;
    const bool sunLights = sunDirection(scene, hour).y >= moonDirection(scene, hour).y;
    auto ramp = [](float x, float a, float b) {
      const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
      return t * t * (3.f - 2.f * t);
    };
    const float low = 1.f - ramp(elev, 8.f, 35.f);
    const float r = weather.rain;
    const float rainLight = r <= 0.3f ? 1.f + 2.5f * r : std::max(0.f, 1.75f * (1.f - (r - 0.3f) / 0.15f));
    lighting.rays = sunLights ? ramp(elev, 0.f, 3.f) * (0.1f + low) * rainLight : 0.f;
    lighting.rayColour = glm::mix(glm::vec3{1.f, 0.97f, 0.86f}, glm::vec3{1.f, 0.8f, 0.56f}, 1.f - ramp(elev, 6.f, 24.f));
  }

  // Firefly light, like the firelight: direct sun drowns it out, so by day
  // it shows only in the shade, and at night it lights all round them.
  {
    const float dark = std::clamp((8.f - lighting.sunElevationDegrees) / 16.f, 0.f, 1.f);
    lighting.fireflyLight = 1.4f;
    lighting.fireflyDim = 0.95f * (1.f - dark);
    for (int s = 0; s < kSurfCount; ++s)
      lighting.fireflyBand[s] = glm::mix(noon.palette[s].band[2], glm::vec3{0.85f, 1.f, 0.5f}, 0.5f);
  }

  // Firelight: a warm pool round each fire. By day direct sun drowns it
  // out, so it shows only in the shade round the fire; once the sun is down
  // it lights everything near and carries farther. Each surface's colour in
  // full firelight is its midday lit colour pulled toward a golden orange.
  {
    const float dark = std::clamp((8.f - lighting.sunElevationDegrees) / 16.f, 0.f, 1.f);
    lighting.fireLight = 1.5f;
    lighting.fireDim = 0.9f * (1.f - dark);
    lighting.fireReach = 6.f + 3.5f * dark;
    for (int s = 0; s < kSurfCount; ++s)
      lighting.fireBand[s] = glm::mix(noon.palette[s].band[2] * 1.05f, glm::vec3{1.f, 0.52f, 0.34f}, 0.55f);
  }

  // A lightning flash: a second light from the flash's direction with hard
  // shadows, blended over the sun or moon by its strength, and the overcast
  // drops away while it lasts. Lit faces take the midday colours washed out
  // to a cold blue-white; the shadow side stays close to the storm's dark.
  if (weather.flash > 0.f) {
    const float f = weather.flash;
    auto cold = [](glm::vec3 c) {
      const float l = glm::dot(c, glm::vec3{0.299f, 0.587f, 0.114f});
      return glm::mix(c, glm::vec3{l}, 0.55f) * glm::vec3{0.86f, 0.95f, 1.18f};
    };
    for (int s = 0; s < kSurfCount; ++s)
      for (int b = 0; b < 3; ++b) {
        Palette& p = lighting.palette[s];
        const Palette& d = noon.palette[s];
        const float k = f * (b == 0 ? 0.35f : 1.f);
        p.band[b] = glm::mix(p.band[b], cold(d.band[b]), k);
        p.detail[b] = glm::mix(p.detail[b], cold(d.detail[b]), k);
        p.tip[b] = glm::mix(p.tip[b], cold(d.tip[b]), k);
      }
    lighting.rim = glm::mix(lighting.rim, cold(noon.rim), f);
    lighting.ink = glm::mix(lighting.ink, noon.ink, f);
    lighting.flashDir = weather.flashDir;
    lighting.overcast *= 1.f - f;
    lighting.flash = f;
  }
  return lighting;
}

}  // namespace pxe
