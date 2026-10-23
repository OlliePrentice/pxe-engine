#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace pxe {

// The height of the highest surface over each cell of a grid on the
// ground, for things that move over the scene - a rolling fireball - to
// follow the ground and stop at walls. Built once from triangles.
class HeightMap {
 public:
  static constexpr float kNone = -1e9f;

  // `triangles` holds three corners per triangle. Walls seen edge-on from
  // above add nothing.
  void build(const std::vector<glm::vec3>& triangles, float cell = 0.2f);
  bool empty() const { return heights_.empty(); }
  // The grid's corners (xz).
  glm::vec2 min() const { return min_; }
  glm::vec2 max() const { return min_ + cell_ * glm::vec2{static_cast<float>(w_), static_cast<float>(h_)}; }
  // The highest surface over the cell holding xz, or kNone.
  float cellTop(glm::vec2 xz) const;
  // Smooth height at xz, blended from the cells round it, so something
  // moving over the ground rises and falls without steps; kNone off the map.
  float heightAt(glm::vec2 xz) const;
  // Where a ray from `origin` along unit `dir` first meets the surface
  // (heightAt), within `maxDist` metres; false if it misses.
  bool raycast(glm::vec3 origin, glm::vec3 dir, float maxDist, glm::vec3& hit) const;

 private:
  float at(int x, int z) const;

  glm::vec2 min_{0.f};
  float cell_ = 0.2f;
  int w_ = 0, h_ = 0;
  std::vector<float> heights_;
};

}  // namespace pxe
