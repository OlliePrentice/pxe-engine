#include "pxe/triangle_grid.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace pxe {

std::vector<Triangle> trianglesOf(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
  std::vector<Triangle> out;
  out.reserve(indices.size() / 3);
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    Triangle t;
    for (int k = 0; k < 3; ++k) {
      const Vertex& v = vertices[indices[i + static_cast<std::size_t>(k)]];
      t.p[k] = {v.px, v.py, v.pz};
      t.n[k] = {v.nx, v.ny, v.nz};
    }
    out.push_back(t);
  }
  return out;
}

TriangleGrid::TriangleGrid(std::vector<Triangle> triangles, float cell) : tris_(std::move(triangles)), cell_(cell) {
  for (const Triangle& t : tris_) {
    for (const glm::vec3& p : t.p) {
      min_ = glm::min(min_, glm::vec2(p.x, p.z));
      max_ = glm::max(max_, glm::vec2(p.x, p.z));
    }
  }
  w_ = std::max(1, static_cast<int>(std::ceil((max_.x - min_.x) / cell_)));
  h_ = std::max(1, static_cast<int>(std::ceil((max_.y - min_.y) / cell_)));
  buckets_.resize(static_cast<std::size_t>(w_ * h_));
  for (std::size_t i = 0; i < tris_.size(); ++i) {
    const Triangle& t = tris_[i];
    glm::vec2 lo{1e9f}, hi{-1e9f};
    for (const glm::vec3& p : t.p) {
      lo = glm::min(lo, glm::vec2(p.x, p.z));
      hi = glm::max(hi, glm::vec2(p.x, p.z));
    }
    const auto [x0, z0] = cellOf(lo);
    const auto [x1, z1] = cellOf(hi);
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x) buckets_[static_cast<std::size_t>(z * w_ + x)].push_back(i);
  }
}

std::optional<TriangleGrid::Hit> TriangleGrid::at(float x, float z) const {
  if (x < min_.x || z < min_.y || x > max_.x || z > max_.y) return std::nullopt;
  const auto [cx, cz] = cellOf({x, z});
  for (std::size_t i : buckets_[static_cast<std::size_t>(cz * w_ + cx)]) {
    const Triangle& t = tris_[i];
    const glm::vec2 a{t.p[0].x, t.p[0].z}, b{t.p[1].x, t.p[1].z}, c{t.p[2].x, t.p[2].z};
    const float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (std::fabs(d) < 1e-9f) continue;
    const float u = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (z - c.y)) / d;
    const float v = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (z - c.y)) / d;
    const float w = 1.f - u - v;
    if (u < -1e-5f || v < -1e-5f || w < -1e-5f) continue;
    return Hit{t.p[0] * u + t.p[1] * v + t.p[2] * w, glm::normalize(t.n[0] * u + t.n[1] * v + t.n[2] * w)};
  }
  return std::nullopt;
}

std::pair<int, int> TriangleGrid::cellOf(glm::vec2 p) const {
  return {std::clamp(static_cast<int>((p.x - min_.x) / cell_), 0, w_ - 1),
          std::clamp(static_cast<int>((p.y - min_.y) / cell_), 0, h_ - 1)};
}

}  // namespace pxe
