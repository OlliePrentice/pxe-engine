#include "pxe/shelter.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace pxe {

void ShelterMap::build(const std::vector<glm::vec3>& triangles, float cell) {
  cell_ = cell;
  start_.clear();
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

  // Each triangle's height at every cell centre it covers, then grouped by cell.
  std::vector<std::pair<int, float>> hits;
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
        const float v = ((c.z - a.z) * (px - c.x) + (a.x - c.x) * (pz - c.z)) / d;
        if (u < 0.f || v < 0.f || u + v > 1.f) continue;
        hits.emplace_back(z * w_ + x, a.y * u + b.y * v + c.y * (1.f - u - v));
      }
  }
  std::sort(hits.begin(), hits.end());
  start_.assign(static_cast<std::size_t>(w_ * h_ + 1), 0);
  for (const auto& hit : hits) ++start_[static_cast<std::size_t>(hit.first + 1)];
  for (std::size_t i = 1; i < start_.size(); ++i) start_[i] += start_[i - 1];
  heights_.reserve(hits.size());
  for (const auto& hit : hits) heights_.push_back(hit.second);
}

bool ShelterMap::roofed(glm::vec2 xz, float from, float to) const {
  if (w_ == 0) return false;
  const int x = static_cast<int>(std::floor((xz.x - min_.x) / cell_));
  const int z = static_cast<int>(std::floor((xz.y - min_.y) / cell_));
  if (x < 0 || z < 0 || x >= w_ || z >= h_) return false;
  const auto cell = static_cast<std::size_t>(z * w_ + x);
  for (int i = start_[cell]; i < start_[cell + 1]; ++i) {
    const float y = heights_[static_cast<std::size_t>(i)];
    if (y >= from && y <= to) return true;
  }
  return false;
}

int ShelterMap::coveredLines(glm::vec3 p) const {
  int covered = 0;
  for (int i = 0; i < 9; ++i) {
    const float t = 6.2831853f * static_cast<float>(i) / 8.f;
    const glm::vec2 q = glm::vec2{p.x, p.z} + (i == 8 ? glm::vec2{0.f} : kRadius * glm::vec2{std::cos(t), std::sin(t)});
    covered += roofed(q, p.y + kLow, p.y + kHigh);
  }
  return covered;
}

}  // namespace pxe
