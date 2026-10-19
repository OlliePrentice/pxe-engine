#include "pxe/fireflies.hpp"

#include <cmath>
#include <cstdint>

namespace pxe {
namespace {

float hash(std::uint32_t i, std::uint32_t salt) {
  std::uint32_t h = i * 747796405u + salt * 2891336453u;
  h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
  h = (h >> 22u) ^ h;
  return static_cast<float>(h & 0xffffff) / 16777216.f;
}

}  // namespace

std::vector<Firefly> firefliesAt(const std::vector<glm::vec3>& homes, float time) {
  std::vector<Firefly> out;
  out.reserve(homes.size());
  for (std::size_t n = 0; n < homes.size(); ++n) {
    const auto i = static_cast<std::uint32_t>(n);
    // A slow lazy loop round home, a metre or so across, bobbing up and down.
    const float a = 0.13f + 0.12f * hash(i, 1), b = 0.11f + 0.1f * hash(i, 2), c = 0.3f + 0.25f * hash(i, 3);
    const float r = 0.7f + 0.8f * hash(i, 4);
    const float p = 6.2831853f * hash(i, 5), q = 6.2831853f * hash(i, 6);
    const glm::vec3 offset{r * std::sin(time * a + p) + 0.3f * std::sin(time * a * 2.7f + q),
                           0.25f * std::sin(time * c + q),
                           r * std::cos(time * b + q) + 0.3f * std::cos(time * b * 2.3f + p)};
    // Blink: lit for a second or two out of every few, easing on and off,
    // each on its own beat.
    const float period = 2.5f + 3.f * hash(i, 7);
    const float phase = std::fmod(time / period + hash(i, 8), 1.f);
    const float on = 0.4f + 0.25f * hash(i, 9);  // share of the cycle it glows
    float glow = 0.f;
    if (phase < on) glow = std::sin(3.14159265f * phase / on);
    out.push_back({homes[n] + offset, glow});
  }
  return out;
}

}  // namespace pxe
