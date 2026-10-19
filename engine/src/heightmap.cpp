#include "pxe/heightmap.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>

namespace pxe {

void HeightMap::build(const std::vector<glm::vec3>& triangles, float cell) {
  cell_ = cell;
  heights_.clear();
  w_ = h_ = 0;
  if (triangles.size() < 3) return;
  glm::vec2 lo{1e9f}, hi{-1e9f};
  for (const glm::vec3& p : triangles) {
    lo = glm::min(lo, glm::vec2{p.x, p.z});
    hi = glm::max(hi, glm::vec2{p.x, p.z});
  }
  min_ = lo;
  w_ = static_cast<int>(std::ceil((hi.x - lo.x) / cell_)) + 1;
  h_ = static_cast<int>(std::ceil((hi.y - lo.y) / cell_)) + 1;
  heights_.assign(static_cast<std::size_t>(w_ * h_), kNone);
  for (std::size_t i = 0; i + 2 < triangles.size(); i += 3) {
    const glm::vec3 &a = triangles[i], &b = triangles[i + 1], &c = triangles[i + 2];
    const float d = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
    if (std::fabs(d) < 1e-9f) continue;
    const int x0 = std::max(0, static_cast<int>((std::min({a.x, b.x, c.x}) - min_.x) / cell_));
    const int x1 = std::min(w_ - 1, static_cast<int>((std::max({a.x, b.x, c.x}) - min_.x) / cell_) + 1);
    const int z0 = std::max(0, static_cast<int>((std::min({a.z, b.z, c.z}) - min_.y) / cell_));
    const int z1 = std::min(h_ - 1, static_cast<int>((std::max({a.z, b.z, c.z}) - min_.y) / cell_) + 1);
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x) {
        const float px = min_.x + (static_cast<float>(x) + 0.5f) * cell_;
        const float pz = min_.y + (static_cast<float>(z) + 0.5f) * cell_;
        const float u = ((b.z - c.z) * (px - c.x) + (c.x - b.x) * (pz - c.z)) / d;
        const float w = ((c.z - a.z) * (px - c.x) + (a.x - c.x) * (pz - c.z)) / d;
        if (u < 0.f || w < 0.f || u + w > 1.f) continue;
        float& t = heights_[static_cast<std::size_t>(z * w_ + x)];
        t = std::max(t, a.y * u + b.y * w + c.y * (1.f - u - w));
      }
  }
}

float HeightMap::at(int x, int z) const {
  if (x < 0 || z < 0 || x >= w_ || z >= h_) return kNone;
  return heights_[static_cast<std::size_t>(z * w_ + x)];
}

float HeightMap::cellTop(glm::vec2 xz) const {
  if (heights_.empty()) return kNone;
  return at(static_cast<int>(std::floor((xz.x - min_.x) / cell_)), static_cast<int>(std::floor((xz.y - min_.y) / cell_)));
}

float HeightMap::heightAt(glm::vec2 xz) const {
  if (heights_.empty()) return kNone;
  const float fx = (xz.x - min_.x) / cell_ - 0.5f, fz = (xz.y - min_.y) / cell_ - 0.5f;
  const int x = static_cast<int>(std::floor(fx)), z = static_cast<int>(std::floor(fz));
  const float tx = fx - static_cast<float>(x), tz = fz - static_cast<float>(z);
  const float h[4] = {at(x, z), at(x + 1, z), at(x, z + 1), at(x + 1, z + 1)};
  const float wt[4] = {(1.f - tx) * (1.f - tz), tx * (1.f - tz), (1.f - tx) * tz, tx * tz};
  // A cell with nothing in it takes the height of those round it that have.
  float sum = 0.f, weight = 0.f;
  for (int i = 0; i < 4; ++i)
    if (h[i] > kNone) {
      sum += h[i] * wt[i];
      weight += wt[i];
    }
  return weight > 1e-6f ? sum / weight : cellTop(xz);
}

bool HeightMap::raycast(glm::vec3 origin, glm::vec3 dir, float maxDist, glm::vec3& hit) const {
  if (heights_.empty()) return false;
  // March half a cell at a time until the ray is below the surface, then
  // close in on the crossing.
  auto below = [&](float t) {
    const glm::vec3 p = origin + dir * t;
    const float h = heightAt({p.x, p.z});
    return h > kNone && p.y <= h;
  };
  const float stepLen = cell_ * 0.5f;
  float prev = 0.f;
  for (float t = 0.f; t <= maxDist; t += stepLen) {
    if (below(t)) {
      float lo = prev, hi = t;
      for (int i = 0; i < 16; ++i) {
        const float mid = 0.5f * (lo + hi);
        (below(mid) ? hi : lo) = mid;
      }
      hit = origin + dir * hi;
      return true;
    }
    prev = t;
  }
  return false;
}

}  // namespace pxe
